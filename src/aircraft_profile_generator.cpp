#include "aircraft_profile_generator.hpp"
#include "aircraft_profile.hpp"
#include "detail_log.hpp"
#include "md5.h"
#include <array>
#include <iomanip>
#include <map>
#include <set>
#include <stdexcept>

namespace profile {
namespace {
using Point=std::array<double,5>; // object x,y,z and texture u,v
struct Screen {std::array<double,4> uv;std::array<Point,4> corners;};
void require(bool value,const std::string& message) {if(!value){OANS_CONFIG("ERROR "+message);throw std::runtime_error(message);}}
double finite_number(const std::string& text) {
    std::size_t used=0;const auto value=std::stod(text,&used);
    require(used==text.size()&&std::isfinite(value),"Invalid source number: "+text);return value;
}
std::string read(const std::filesystem::path& path) {
    OANS_TRACE_SCOPE();OANS_CONFIG("Reading source: "+path.u8string());
    std::ifstream file(path,std::ios::binary);require(bool(file),"Cannot read source: "+path.u8string());
    std::string text((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    require(!file.bad(),"Source read failed: "+path.u8string());return text;
}
std::string lower(std::string text) {
    std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return text;
}
std::string md5_prefix(const std::filesystem::path& path) {
    OANS_TRACE_SCOPE();OANS_CONFIG("Hashing source for profile ID (first 4 MD5 hex): "+path.u8string());
    std::ifstream file(path,std::ios::binary);require(bool(file),"Cannot hash source: "+path.u8string());
    MD5_CTX context;MD5Init(&context);
    std::array<unsigned char,8192> buffer{};
    while(file.read(reinterpret_cast<char*>(buffer.data()),buffer.size())||file.gcount())
        MD5Update(&context,buffer.data(),static_cast<unsigned short>(file.gcount()));
    require(file.eof()&&!file.bad(),"Source hash read failed: "+path.u8string());
    MD5Final(&context);
    std::ostringstream out;out<<std::hex<<std::setfill('0');
    for(int i=0;i<2;++i)out<<std::setw(2)<<static_cast<unsigned>(context.digest[i]);
    return out.str();
}
struct Source {
    std::filesystem::path cockpit;
    std::map<std::string,std::string> properties;
    std::string attachment,id;
    const std::string& property(const std::string& key) const {
        const auto found=properties.find(key);require(found!=properties.end(),"Missing ACF property: "+key);return found->second;
    }
};
Source resolve_source(const std::filesystem::path& model) {
    OANS_TRACE_SCOPE();OANS_CONFIG("Active ACF: "+model.u8string());
    require(std::filesystem::is_regular_file(model),"Cannot read active ACF: "+model.u8string());
    Source source;
    std::istringstream acf(read(model));std::string line;
    while(std::getline(acf,line)) {
        std::istringstream row(line);std::string tag,key,value;
        if(row>>tag>>key&&tag=="P") {std::getline(row>>std::ws,value);if(!value.empty()&&value.back()=='\r')value.pop_back();source.properties[key]=value;}
    }
    std::vector<std::string> preferred, fallback;
    for(const auto& item:source.properties)if(item.first.rfind("_obja/",0)==0&&item.first.find("/_v10_att_file_stl")!=std::string::npos) {
        const auto path=std::filesystem::u8path(item.second);
        const auto filename=lower(path.filename().u8string());
        if(lower(path.extension().u8string())!=".obj"||filename.find("cockpit")==std::string::npos)continue;
        auto& candidates=filename.find("inn")!=std::string::npos?preferred:fallback;
        OANS_CONFIG("Cockpit candidate property="+item.first+" path="+item.second+" priority="+(filename.find("inn")!=std::string::npos?"cockpit+inn":"cockpit fallback"));
        candidates.push_back(item.first.substr(0,item.first.find("/_v10_att_file_stl")));
    }
    const auto& candidates=preferred.empty()?fallback:preferred;
    require(!candidates.empty(),"Cockpit OBJ attachment not found in ACF (prefer filenames containing cockpit and inn)");
    require(candidates.size()==1,"Ambiguous cockpit attachment");
    source.attachment=candidates.front();
    auto cockpit=std::filesystem::u8path(source.property(source.attachment+"/_v10_att_file_stl"));
    require(!cockpit.is_absolute(),"Absolute cockpit attachment path is unsupported");
    // ACF attached-object paths are relative to the aircraft objects directory.
    source.cockpit=(model.parent_path()/"objects"/cockpit).lexically_normal();
    OANS_CONFIG("Selected OBJ: "+source.cockpit.u8string()+" attachment="+source.attachment+" reason="+(preferred.empty()?"unique cockpit fallback":"unique cockpit+inn"));
    source.id=model.stem().u8string()+"-"+md5_prefix(model)+"-"+md5_prefix(source.cockpit);
    OANS_CONFIG("Profile ID="+source.id+" rule=active ACF stem + ACF MD5[0:4] + OBJ MD5[0:4]");
    return source;
}
AircraftProfile extract(const Source& source,const std::string& icao) {
    OANS_TRACE_SCOPE();OANS_CONFIG("Generating profile; active ICAO="+icao+" ACF ICAO="+source.property("acf/_ICAO"));
    // Try the shared ToLiss six-screen layout using the active aircraft's geometry.
    auto property=[&](const std::string& key)->const std::string& {return source.property(key);};
    require(lower(property("acf/_ICAO"))==lower(icao),"ACF ICAO differs from active aircraft ICAO");
    const auto& attachment=source.attachment;
    for(const auto* axis:{"phi","psi","the"})require(finite_number(property(attachment+"/_v10_att_"+axis+"_ref"))==0,"Rotated cockpit attachment is unsupported");
    std::array<double,3> offset{};
    for(int i=0;i<3;++i)offset[i]=finite_number(property(attachment+"/_v10_att_"+std::string(1,"xyz"[i])+"_acf_prt_ref"))*.3048;
    offset[1]-=finite_number(property("acf/_cgY"))*.3048;
    offset[2]-=finite_number(property("acf/_cgZ"))*.3048;
    const auto text=read(source.cockpit);
    std::istringstream obj(text);std::string line;std::vector<Point> vertices;std::vector<int> indices;
    std::vector<std::vector<int>> conditions;
    std::array<std::vector<Screen>,2> displays;
    while(std::getline(obj,line)) {
        std::istringstream row(line);std::string tag;row>>tag;
        if(tag=="VT") {
            std::array<double,8> values{};
            for(auto& value:values) {std::string token;require(bool(row>>token),"Incomplete OBJ vertex");value=finite_number(token);}
            vertices.push_back({values[0],values[1],values[2],values[6],values[7]});
        } else if(tag=="IDX"||tag=="IDX10") {
            int index;while(row>>index)indices.push_back(index);
            require(row.eof(),"Invalid OBJ index");
        } else if(tag=="ANIM_begin")conditions.emplace_back();
        else if(tag=="ANIM_end") {require(!conditions.empty(),"Unbalanced OBJ animation");conditions.pop_back();}
        else if(tag=="ANIM_hide"&&!conditions.empty()) {
            double start,end;std::string ref;
            require(bool(row>>start>>end>>ref),"Invalid OBJ hide condition");
            if(ref=="AirbusFBW/DisplayResolutionFallback") {
                const bool normal=start<=0&&end>=0, fallback=start<=1&&end>=1;
                require(normal!=fallback,"Unsupported display hide condition");
                conditions.back().push_back(normal?1:0); // the visible resolution
            }
        } else if(tag=="TRIS") {
            int first,count;require(bool(row>>first>>count),"Invalid OBJ triangle range");
            int mode=-1;for(const auto& block:conditions)for(int visible:block)mode=visible;
            if(mode<0||count!=36)continue;
            require(displays[mode].empty(),"Multiple candidate six-screen batches for one resolution");
            require(first>=0&&static_cast<std::size_t>(first)+36<=indices.size(),"OBJ screen index range out of bounds");
            for(int screen=0;screen<6;++screen) {
                std::set<Point> unique;
                for(int i=first+screen*6;i<first+screen*6+6;++i) {
                    require(indices[i]>=0&&static_cast<std::size_t>(indices[i])<vertices.size(),"OBJ vertex index out of bounds");
                    unique.insert(vertices[indices[i]]);
                }
                require(unique.size()==4,"Screen must have four distinct corners");
                std::vector<Point> points(unique.begin(),unique.end());
                std::sort(points.begin(),points.end(),[](const Point& a,const Point& b){return a[4]!=b[4]?a[4]>b[4]:a[3]<b[3];});
                Screen result;result.corners={points[0],points[1],points[3],points[2]};
                result.uv={1,1,0,0};
                for(const auto& point:points) {
                    result.uv[0]=std::min(result.uv[0],point[3]);result.uv[1]=std::min(result.uv[1],point[4]);
                    result.uv[2]=std::max(result.uv[2],point[3]);result.uv[3]=std::max(result.uv[3],point[4]);
                }
                require(result.uv[0]>=0&&result.uv[1]>=0&&result.uv[2]<=1&&result.uv[3]<=1&&result.uv[0]<result.uv[2]&&result.uv[1]<result.uv[3],"Invalid screen UV rectangle");
                displays[mode].push_back(result);
            }
        }
    }
    require(conditions.empty(),"Unbalanced OBJ animation at end of file");
    require(displays[0].size()==6&&displays[1].size()==6,"Expected normal and fallback six-screen display batches");
    OANS_CONFIG("OBJ parsed vertices="+std::to_string(vertices.size())+" indices="+std::to_string(indices.size())+" normal/fallback screens="+std::to_string(displays[0].size())+"/"+std::to_string(displays[1].size()));
    AircraftProfile result{};
    for(int side=0;side<2;++side) {
        const std::string label=side==0?"Left":"Right";
        std::smatch match;
        const std::regex nd("ckpt/lights/ndInner"+label+"/anim[\\s\\S]*?AirbusFBW/DUBrightness\\[([0-9]+)\\]");
        require(std::regex_search(text,match,nd),"ND brightness manipulator not found: "+label);
        result.nd_brightness[side]=std::stoi(match[1].str());
        require(result.nd_brightness[side]>=0&&result.nd_brightness[side]<=255,"ND brightness index out of supported range");
        // Captain PFD manipulator has no named animation in this ToLiss layout.
        result.pfd_brightness[side]=side==0?0:2;
        const std::regex pfd("ATTR_manip_drag_axis[^\\r\\n]*AirbusFBW/DUBrightness\\["+std::to_string(result.pfd_brightness[side])+"\\]");
        require(std::regex_search(text,pfd),"Expected ToLiss PFD brightness manipulator missing");
        for(int mode=0;mode<2;++mode)for(int j=0;j<4;++j) {
            result.nd_uv[mode][side][j]=displays[mode][side*2+1].uv[j];
            result.pfd_uv[mode][side][j]=displays[mode][side*2].uv[j];
        }
        for(int corner=0;corner<4;++corner)for(int axis=0;axis<3;++axis)
            result.nd_corners[side][corner][axis]=displays[0][side*2+1].corners[corner][axis]+offset[axis];
    }
    return result;
}
void write(std::ostream& out,const AircraftProfile& p) {
    out<<std::setprecision(17)<<"{\n  \"nd_brightness\": ["<<p.nd_brightness[0]<<", "<<p.nd_brightness[1]<<"],\n  \"pfd_brightness\": ["<<p.pfd_brightness[0]<<", "<<p.pfd_brightness[1]<<"],\n";
    auto uv=[&](const char* key,const double (&values)[2][2][4]) {
        out<<"  \""<<key<<"\": [\n";
        for(int mode=0;mode<2;++mode) {
            out<<"    [";
            for(int side=0;side<2;++side) {if(side)out<<", ";out<<'[';for(int i=0;i<4;++i){if(i)out<<", ";out<<values[mode][side][i];}out<<']';}
            out<<']'<<(mode==0?",":"")<<'\n';
        }
        out<<"  ],\n";
    };
    uv("nd_uv",p.nd_uv);uv("pfd_uv",p.pfd_uv);out<<"  \"nd_corners\": [\n";
    for(int side=0;side<2;++side) {
        out<<"    [";for(int corner=0;corner<4;++corner){if(corner)out<<", ";out<<'[';for(int axis=0;axis<3;++axis){if(axis)out<<", ";out<<p.nd_corners[side][corner][axis];}out<<']';}
        out<<']'<<(side==0?",":"")<<'\n';
    }
    out<<"  ]\n}\n";
}
}
bool identify(const std::filesystem::path& model,std::string& id,std::string& error) {
    id.clear();error.clear();
    try {id=resolve_source(model).id;return true;}
    catch(const std::exception& e) {error=e.what();return false;}
}
bool load_for_aircraft(const std::filesystem::path& directory,const std::filesystem::path& model,const std::string& icao,
                       std::filesystem::path& loaded,std::string& error) {
    OANS_TRACE_SCOPE();OANS_CONFIG("Profile lookup directory="+directory.u8string()+" ICAO="+icao+" model="+model.u8string());
    std::filesystem::path temporary;
    current={};loaded.clear();error.clear();
    try {
        require(valid_icao(icao),"Invalid aircraft ICAO: "+icao);
        Source source;std::string source_error;
        try {source=resolve_source(model);}catch(const std::exception& e) {source_error=e.what();OANS_CONFIG("Cannot derive source ID: "+source_error+"; checking distributed ICAO fallback");}
        const auto target=source_error.empty()?directory/std::filesystem::u8path(source.id+".json"):std::filesystem::path{};
        auto load_selected=[&](const std::filesystem::path& path) {
            OANS_CONFIG("Loading selected JSON: "+path.u8string());
            if(!load_file(path,error)){OANS_CONFIG("Profile rejected: "+error+"; existing file preserved, no fallback/regeneration");return false;}
            OANS_CONFIG("Profile loaded and schema validated: "+path.u8string());
            loaded=path;return true;
        };
        if(!target.empty()) {
            if(std::filesystem::exists(target)){OANS_CONFIG("Found ID profile (highest priority): "+target.u8string());return load_selected(target);}
            OANS_CONFIG("ID profile not found: "+target.u8string());
        }
        const auto base=directory/(lower(icao)+".json");
        if(std::filesystem::exists(base)){OANS_CONFIG("Found distributed ICAO fallback: "+base.u8string());return load_selected(base);}
        OANS_CONFIG("Distributed ICAO profile not found: "+base.u8string()+"; attempting generation");
        require(source_error.empty(),source_error);
        const auto data=extract(source,icao);
        std::filesystem::create_directories(directory);
        temporary=target;temporary+=".tmp";
        OANS_CONFIG("Writing generated profile temporary="+temporary.u8string()+" target="+target.u8string());
        {std::ofstream file(temporary);require(bool(file),"Cannot create generated profile: "+temporary.u8string());write(file,data);file.close();require(bool(file),"Generated profile write failed: "+temporary.u8string());}
        // Publish only a complete file. On Windows rename fails if a target appeared meanwhile.
        std::filesystem::rename(temporary,target);temporary.clear();OANS_CONFIG("Generated profile committed: "+target.u8string());return load_selected(target);
    }catch(const std::exception& e) {
        if(!temporary.empty()){std::error_code ignored;std::filesystem::remove(temporary,ignored);}
        error=e.what();OANS_CONFIG("Profile lookup/generation failed: "+error+"; no new profile published");return false;
    }
}
}
