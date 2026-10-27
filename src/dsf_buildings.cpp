#include "detail_log.hpp"
#include "dsf_buildings.hpp"
#include "DSFLib.h"
extern "C" {
#include "7z.h"
#include "7zAlloc.h"
#include "7zCrc.h"
#include "7zFile.h"
}
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <locale>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace oans {
namespace {
constexpr size_t max_tile_bytes=512*1024*1024;
struct Bounds {double west=180,south=90,east=-180,north=-90;};
struct Point {double lon,lat,cx,cy;};
struct Building {std::string resource,kind;double height;std::vector<Point> ring;};
struct Exclusion {Bounds box;bool contains(const Point& p) const {
    return p.lon>=box.west&&p.lon<=box.east&&p.lat>=box.south&&p.lat<=box.north;
}};
struct Tile {
    const std::atomic<bool>* cancel=nullptr;
    Bounds bounds;
    std::vector<std::string> definitions;
    std::vector<Building> buildings;
    std::vector<Exclusion> exclusions;
    std::vector<std::string> filters;
    std::vector<std::vector<Point>> rings;
    std::string airport,resource,kind;
    bool overridden=false,filtered=false,property_filtered=false,overlay=false;
    int depth=0;
    double height=0;
};
std::string lower(std::string s) {
    std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return s;
}
bool regular(const std::filesystem::path& p) {std::error_code e;return std::filesystem::is_regular_file(p,e);}
std::string trim(std::string s) {
    const auto a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");
    return a==std::string::npos?std::string{}:s.substr(a,b-a+1);
}
std::string tile_name(int lat,int lon) {
    std::ostringstream o;o.imbue(std::locale::classic());
    o<<(lat<0?'-':'+')<<std::setw(2)<<std::setfill('0')<<std::abs(lat)
     <<(lon<0?'-':'+')<<std::setw(3)<<std::abs(lon);return o.str();
}
bool valid(double lon,double lat) {return std::isfinite(lon)&&std::isfinite(lat)&&std::abs(lon)<=180&&std::abs(lat)<=90;}
double area(const std::vector<Point>& ring) {
    if(ring.size()<3)return 0;
    const auto origin=ring.front();double sum=0;
    for(size_t i=0;i<ring.size();++i) {
        const auto a=local(ring[i].lat,ring[i].lon,origin.lat,origin.lon);
        const auto& q=ring[(i+1)%ring.size()];const auto b=local(q.lat,q.lon,origin.lat,origin.lon);
        sum+=a.x*b.y-a.y*b.x;
    }
    return std::abs(sum)*.5;
}
std::vector<Point> flatten(std::vector<Point> v) {
    if(v.size()>1&&std::abs(v.front().lon-v.back().lon)<1e-10&&std::abs(v.front().lat-v.back().lat)<1e-10)v.pop_back();
    if(v.size()<3)return {};
    std::vector<Point> out{v.front()};
    for(size_t i=0;i<v.size();++i) {
        const auto a=v[i],b=v[(i+1)%v.size()];
        const double cx=2*a.lon-a.cx,cy=2*a.lat-a.cy;
        const bool curve=std::abs(cx-a.lon)>1e-10||std::abs(cy-a.lat)>1e-10||std::abs(b.cx-b.lon)>1e-10||std::abs(b.cy-b.lat)>1e-10;
        const int n=curve?12:1;
        for(int k=1;k<=n;++k) {
            const double t=double(k)/n,u=1-t;
            out.push_back({u*u*u*a.lon+3*u*u*t*cx+3*u*t*t*b.cx+t*t*t*b.lon,
                           u*u*u*a.lat+3*u*u*t*cy+3*u*t*t*b.cy+t*t*t*b.lat,0,0});
        }
    }
    return out;
}
bool canceled(void* ref) {return static_cast<Tile*>(ref)->cancel->load();}
int ignore_def(const char*,void* ref) {return !canceled(ref);}
int polygon_def(const char* path,void* ref) {static_cast<Tile*>(ref)->definitions.emplace_back(path);return !canceled(ref);}
bool next_pass(int,void* ref) {return !canceled(ref);}
void property(const char* key,const char* value,void* ref) {
    auto& t=*static_cast<Tile*>(ref);const std::string name=key;
    if(name=="sim/overlay")t.overlay=std::string(value)=="1";
    if(name=="sim/filter/aptid") {
        t.filters.emplace_back(value);t.property_filtered=t.overridden&&t.airport==value;
    }
    if(name=="sim/exclude_fac"&&!t.property_filtered) {
        std::string s=value;std::replace(s.begin(),s.end(),'/',' ');
        std::istringstream in(s);in.imbue(std::locale::classic());Exclusion e;
        if(in>>e.box.west>>e.box.south>>e.box.east>>e.box.north)
            if(valid(e.box.west,e.box.south)&&valid(e.box.east,e.box.north)&&e.box.west<=e.box.east&&e.box.south<=e.box.north)t.exclusions.push_back(e);
    }
}
void filter(int index,void* ref) {
    auto& t=*static_cast<Tile*>(ref);
    t.filtered=t.overridden&&index>=0&&size_t(index)<t.filters.size()&&t.filters[index]==t.airport;
}
void begin_polygon(unsigned int index,unsigned short height,int depth,void* ref) {
    auto& t=*static_cast<Tile*>(ref);t.rings.clear();t.kind.clear();t.depth=depth;t.height=height;
    if(t.filtered||index>=t.definitions.size()||depth<2)return;
    t.resource=t.definitions[index];const auto s=lower(t.resource);
    if(s.size()<4||s.substr(s.size()-4)!=".fac")return;
    for(const auto word:{"bridge","jetway","fence","roof","canopy","awning","interior"})if(s.find(word)!=std::string::npos)return;
    const bool terminal=s.find("terminal")!=std::string::npos||s.find("term_building")!=std::string::npos;
    const bool known=terminal||s.find("building")!=std::string::npos||s.find("hangar")!=std::string::npos;
    if(!known&&height<2)return;
    t.kind=terminal?"terminal":"building";
}
void begin_ring(void* ref) {auto& t=*static_cast<Tile*>(ref);if(!t.kind.empty())t.rings.emplace_back();}
void point(double* p,void* ref) {
    auto& t=*static_cast<Tile*>(ref);if(t.kind.empty()||t.rings.empty()||!valid(p[0],p[1]))return;
    const bool curve=t.depth>=4;const double cx=curve?p[t.depth-2]:p[0],cy=curve?p[t.depth-1]:p[1];
    t.rings.back().push_back({p[0],p[1],valid(cx,cy)?cx:p[0],valid(cx,cy)?cy:p[1]});
}
void end_ring(void*) {}
void end_polygon(void* ref) {
    auto& t=*static_cast<Tile*>(ref);if(t.kind.empty()||t.cancel->load())return;
    std::vector<Point> best;double best_area=0;
    for(auto& ring:t.rings) {auto flat=flatten(std::move(ring));const double a=area(flat);if(a>best_area){best_area=a;best=std::move(flat);}}
    if(best_area<150)return;
    Bounds b;
    for(const auto& p:best) {b.west=std::min(b.west,p.lon);b.east=std::max(b.east,p.lon);b.south=std::min(b.south,p.lat);b.north=std::max(b.north,p.lat);}
    if(b.east<t.bounds.west||b.west>t.bounds.east||b.north<t.bounds.south||b.south>t.bounds.north)return;
    t.buildings.push_back({t.resource,t.kind,t.height,std::move(best)});
}
// Unicode-aware file IO; compressed DSFs are decoded in-process, without an
// installed Python, DSFTool, shell command, or temporary conversion process.
std::vector<char> read_tile(const std::filesystem::path& path,const std::atomic<bool>& cancel) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in)throw std::runtime_error("cannot open DSF");
    const auto size=in.tellg();if(size<28||size>std::streamoff(max_tile_bytes))throw std::runtime_error("DSF size is outside limit");
    std::vector<char> data(static_cast<size_t>(size));in.seekg(0);in.read(data.data(),size);
    if(!in)throw std::runtime_error("cannot read DSF");
    if(std::memcmp(data.data(),"XPLNEDSF",8)==0)return data;
    if(std::memcmp(data.data(),"7z\xBC\xAF\x27\x1C",6)!=0)throw std::runtime_error("unknown DSF signature");
    if(cancel.load())return {};
    static std::once_flag crc;std::call_once(crc,[]{CrcGenerateTable();});
    struct Archive {
        CSzArEx db{};CFileInStream file{};CLookToRead2 look{};
        ISzAlloc alloc{SzAlloc,SzFree};Byte* output=nullptr;
        Archive() {SzArEx_Init(&db);File_Construct(&file.file);}
        ~Archive() {ISzAlloc_Free(&alloc,output);SzArEx_Free(&db,&alloc);ISzAlloc_Free(&alloc,look.buf);File_Close(&file.file);}
    } ar;
#ifdef _WIN32
    if(InFile_OpenW(&ar.file.file,path.c_str()))throw std::runtime_error("cannot open compressed DSF");
#else
    if(InFile_Open(&ar.file.file,path.c_str()))throw std::runtime_error("cannot open compressed DSF");
#endif
    FileInStream_CreateVTable(&ar.file);LookToRead2_CreateVTable(&ar.look,False);
    ar.look.bufSize=1<<18;ar.look.buf=static_cast<Byte*>(ISzAlloc_Alloc(&ar.alloc,ar.look.bufSize));
    if(!ar.look.buf)throw std::bad_alloc();
    ar.look.realStream=&ar.file.vt;LookToRead2_Init(&ar.look);
    if(SzArEx_Open(&ar.db,&ar.look.vt,&ar.alloc,&ar.alloc))throw std::runtime_error("invalid compressed DSF archive");
    UInt32 member=0;
    while(member<ar.db.NumFiles&&SzArEx_IsDir(&ar.db,member))++member;
    if(member==ar.db.NumFiles||SzArEx_GetFileSize(&ar.db,member)>max_tile_bytes)throw std::runtime_error("compressed DSF size is outside limit");
    for(UInt32 f=0;f<ar.db.db.NumFolders;++f)if(SzAr_GetFolderUnpackSize(&ar.db.db,f)>max_tile_bytes)throw std::runtime_error("DSF solid block exceeds size limit");
    UInt32 block=0xFFFFFFFF;size_t output_size=0,offset=0,processed=0;
    if(cancel.load())return {};
    if(SzArEx_Extract(&ar.db,&ar.look.vt,member,&block,&ar.output,&output_size,&offset,&processed,&ar.alloc,&ar.alloc))throw std::runtime_error("cannot decompress DSF");
    if(cancel.load())return {};
    return std::vector<char>(reinterpret_cast<char*>(ar.output+offset),reinterpret_cast<char*>(ar.output+offset+processed));
}
// Validate containers, strings and encoded point planes before handing their
// pointers to the upstream reader. A truncated scenery must become a fallback,
// never an out-of-bounds decoder read inside the simulator process.
unsigned le32(const char* p) {
    const auto* b=reinterpret_cast<const unsigned char*>(p);
    return unsigned(b[0])|(unsigned(b[1])<<8)|(unsigned(b[2])<<16)|(unsigned(b[3])<<24);
}
void validate_atoms(const char* first,const char* end,size_t& decoded_bytes) {
    for(const char* p=first;p<end;) {
        if(end-p<8)throw std::runtime_error("truncated DSF atom header");
        const auto size=le32(p+4);if(size<8||size>size_t(end-p))throw std::runtime_error("invalid DSF atom size");
        const std::string id(p,4);const char* data=p+8;const char* stop=p+size;
        if(id=="DAEH"||id=="NFED"||id=="DOEG"||id=="SMED")validate_atoms(data,stop,decoded_bytes);
        else if(id=="PORP"||id=="TRET"||id=="TJBO"||id=="YLOP"||id=="WTEN"||id=="NMED") {
            if(data!=stop&&stop[-1]!='\0')throw std::runtime_error("unterminated DSF string table");
        } else if(id=="LOOP"||id=="23OP") {
            if(stop-data<5)throw std::runtime_error("truncated DSF point pool");
            const size_t count=le32(data),planes=static_cast<unsigned char>(data[4]);const size_t width=id=="LOOP"?2:4;
            if(planes>32||(planes==0&&count!=0)||(planes&&count>max_tile_bytes/8/planes))throw std::runtime_error("invalid DSF point pool dimensions");
            decoded_bytes+=count*planes*8;if(decoded_bytes>max_tile_bytes)throw std::runtime_error("decoded DSF pools exceed memory limit");
            auto q=data+5;
            for(size_t plane=0;plane<planes;++plane) {
                if(q==stop)throw std::runtime_error("truncated DSF plane encoding");
                const auto mode=static_cast<unsigned char>(*q++);if(mode>3)throw std::runtime_error("unknown DSF plane encoding");
                if(mode<2) {
                    if(size_t(stop-q)<count*width)throw std::runtime_error("truncated raw DSF plane");
                    q+=count*width;
                } else {
                    size_t left=count;
                    while(left) {
                        if(q==stop)throw std::runtime_error("truncated RLE DSF plane");
                        const auto token=static_cast<unsigned char>(*q++);const size_t run=token&127;
                        const size_t bytes=(token&128)?width:run*width;
                        if(!run||run>left||size_t(stop-q)<bytes)throw std::runtime_error("invalid RLE DSF run");
                        q+=bytes;left-=run;
                    }
                }
            }
            if(q!=stop)throw std::runtime_error("trailing DSF pool data");
        } else if((id=="LACS"||id=="23CS")&&(stop-data)%8)throw std::runtime_error("invalid DSF scale table");
        p=stop;
    }
}
std::string quoted(const std::string& s) {
    std::ostringstream o;o<<'"';for(unsigned char c:s) {
        if(c=='"'||c=='\\')o<<'\\'<<c;
        else if(c<32)o<<"\\u00"<<std::hex<<std::setw(2)<<std::setfill('0')<<int(c)<<std::dec;
        else o<<c;
    }o<<'"';return o.str();
}
}

std::vector<std::filesystem::path> dsf_scenery_packs(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> packs;std::set<std::filesystem::path> seen,disabled;
    auto add=[&](std::filesystem::path p){p=p.lexically_normal();if(!disabled.count(p)&&seen.insert(p).second)packs.push_back(p);};
    std::ifstream ini(root/"Custom Scenery/scenery_packs.ini");std::string line;
    if(ini) {
        std::vector<std::filesystem::path> active;
        while(std::getline(ini,line)) {
            line=trim(line);const bool off=line.rfind("SCENERY_PACK_DISABLED ",0)==0;
            if(!off&&line.rfind("SCENERY_PACK ",0)!=0)continue;
            const auto value=trim(line.substr(off?22:13));
            auto path=value=="*GLOBAL_AIRPORTS*"?root/"Global Scenery/Global Airports":std::filesystem::u8path(value);
            if(path.is_relative())path=root/path;
            path=path.lexically_normal();
            // Remove trailing directory separators so duplicate/disabled keys match.
            if(path.filename().empty())path=path.parent_path();
            if(off)disabled.insert(path);else active.push_back(path);
        }
        for(const auto& p:active)add(p);
    } else {
        std::error_code ec;std::vector<std::filesystem::path> found;
        for(std::filesystem::directory_iterator it(root/"Custom Scenery",ec),end;!ec&&it!=end;it.increment(ec))if(it->is_directory())found.push_back(it->path());
        std::sort(found.begin(),found.end());for(const auto& p:found)add(p);
    }
    // The built-in airport marker may be absent in older installations.
    add(root/"Global Scenery/Global Airports");
    add(root/"Global Scenery/X-Plane 12 Global Scenery");
    add(root/"Global Scenery/X-Plane 11 Global Scenery");
    return packs;
}

DsfExtraction extract_runtime_dsf(const std::filesystem::path& root,const IndexEntry& entry,
                                  const std::filesystem::path& cache_directory,const std::atomic<bool>& cancel) {
    DsfExtraction result;
    if(cancel.load())return result;
    const auto airport=load_airport(entry);Bounds bounds;
    auto include=[&](Vec2 p){const double lat=airport.lat+p.y/earth*180/pi;
        const double lon=airport.lon+p.x/(earth*std::max(.01,std::cos(airport.lat*pi/180)))*180/pi;
        bounds.west=std::min(bounds.west,lon);bounds.east=std::max(bounds.east,lon);bounds.south=std::min(bounds.south,lat);bounds.north=std::max(bounds.north,lat);};
    for(const auto& r:airport.runways){include(r.start);include(r.end);}
    for(const auto& p:airport.pavements)for(const auto& ring:p.rings)for(auto v:ring)include(v);
    for(const auto& s:airport.stands)include(s.pos);
    const double dy=800/111320.0,dx=dy/std::max(.01,std::cos(airport.lat*pi/180));
    bounds.west-=dx;bounds.east+=dx;bounds.south-=dy;bounds.north+=dy;
    if(!valid(bounds.west,bounds.south)||!valid(bounds.east,bounds.north)||bounds.east-bounds.west>2||bounds.north-bounds.south>2)
        throw std::runtime_error("airport DSF bounds invalid or exceed two degrees");
    std::vector<Building> buildings;std::vector<Exclusion> exclusions;std::vector<std::string> sources;
    std::set<std::string> base_tiles;std::string errors;
    const auto owner=entry.file.parent_path().parent_path().lexically_normal();
    auto packs=dsf_scenery_packs(root);
    const auto owner_it=std::find(packs.begin(),packs.end(),owner);
    const size_t owner_priority=static_cast<size_t>(owner_it-packs.begin());
    for(size_t priority=0;priority<packs.size()&&!cancel.load();++priority) {
        const auto& pack=packs[priority];std::vector<Exclusion> pack_exclusions;
        for(int lat=int(std::floor(bounds.south));lat<=int(std::floor(bounds.north))&&!cancel.load();++lat)
        for(int lon=int(std::floor(bounds.west));lon<=int(std::floor(bounds.east))&&!cancel.load();++lon) {
            const auto name=tile_name(lat,lon);if(base_tiles.count(name))continue;
            const auto file=pack/"Earth nav data"/tile_name(int(std::floor(lat/10.0))*10,int(std::floor(lon/10.0))*10)/(name+".dsf");
            if(!regular(file))continue;
            try {
                Tile t;t.cancel=&cancel;t.bounds=bounds;t.airport=entry.header_id.empty()?entry.icao:entry.header_id;
                t.overridden=priority>owner_priority;
                OANS_TRACE("DSF tile read/decompress "+file.u8string());
                auto data=read_tile(file,cancel);if(cancel.load())return {};
                if(data.size()<28||std::memcmp(data.data(),"XPLNEDSF",8))throw std::runtime_error("invalid uncompressed DSF header");
                size_t decoded_bytes=0;validate_atoms(data.data()+12,data.data()+data.size()-16,decoded_bytes);
                DSFCallbacks_t cb{};cb.Cancel_f=canceled;cb.NextPass_f=next_pass;
                cb.AcceptTerrainDef_f=ignore_def;cb.AcceptObjectDef_f=ignore_def;cb.AcceptNetworkDef_f=ignore_def;cb.AcceptRasterDef_f=ignore_def;
                cb.AcceptPolygonDef_f=polygon_def;cb.AcceptProperty_f=property;cb.SetFilter_f=filter;
                cb.BeginPolygon_f=begin_polygon;cb.BeginPolygonWinding_f=begin_ring;cb.AddPolygonPoint_f=point;cb.EndPolygonWinding_f=end_ring;cb.EndPolygon_f=end_polygon;
                const int passes[]={dsf_CmdProps|dsf_CmdDefs|dsf_CmdPolys,0};
                OANS_TRACE("DSFReadMem begin bytes="+std::to_string(data.size()));
                const int code=DSFReadMem(data.data(),data.data()+data.size(),&cb,passes,&t);
                OANS_TRACE("DSFReadMem result="+std::to_string(code));
                if(cancel.load())return {};
                if(code)throw std::runtime_error(std::string("DSFLib: ")+dsfErrorMessages[code]);
                ++result.tiles;sources.push_back(file.u8string());
                for(auto& b:t.buildings) {
                    const bool excluded=std::any_of(exclusions.begin(),exclusions.end(),[&](const Exclusion& e){return std::any_of(b.ring.begin(),b.ring.end(),[&](const Point& p){return e.contains(p);});});
                    if(!excluded)buildings.push_back(std::move(b));
                }
                pack_exclusions.insert(pack_exclusions.end(),t.exclusions.begin(),t.exclusions.end());
                if(!t.overlay)base_tiles.insert(name);
            } catch(const std::exception& e) {OANS_TRACE("DSF tile error="+std::string(e.what()));if(!errors.empty())errors+="; ";errors+=file.u8string()+": "+e.what();}
        }
        exclusions.insert(exclusions.end(),pack_exclusions.begin(),pack_exclusions.end());
    }
    if(cancel.load())return {};
    result.buildings=buildings.size();
    result.terminals=std::count_if(buildings.begin(),buildings.end(),[](const Building& b){return b.kind=="terminal";});
    if(buildings.empty()) {result.note="runtime scenery DSF extraction found no usable facade footprints ("+std::to_string(result.tiles)+" tiles)";
        if(!errors.empty())result.note+="; "+errors;
        return result;
    }
    std::filesystem::create_directories(cache_directory);
    result.cache=cache_directory/("dsf_buildings_"+entry.icao+".json");
    auto temporary=result.cache;temporary+=".extracting";
    try {
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out.imbue(std::locale::classic());
        if(!out)throw std::runtime_error("cannot create runtime DSF cache");
        out<<std::setprecision(12)<<"{\"schema\":\"oans-dsf-buildings-v1\",\"icao\":"<<quoted(entry.icao)
           <<",\"source\":\"x-plane-dsf-facade\",\"runtime_extracted\":true,\"sources\":[";
        for(size_t i=0;i<sources.size();++i){if(i)out<<',';out<<quoted(sources[i]);}
        out<<"],\"buildings\":[";
        for(size_t i=0;i<buildings.size();++i) {
            if(cancel.load()){out.close();std::filesystem::remove(temporary);return {};}
            const auto& b=buildings[i];if(i)out<<',';
            out<<"{\"kind\":"<<quoted(b.kind)<<",\"resource\":"<<quoted(b.resource)<<",\"facade_height_m\":"<<b.height<<",\"points\":[";
            for(size_t j=0;j<b.ring.size();++j){if(j)out<<',';out<<'['<<b.ring[j].lon<<','<<b.ring[j].lat<<']';}
            out<<"]}";
        }
        out<<"]}\n";out.close();if(!out)throw std::runtime_error("cannot write runtime DSF cache");
#ifdef _WIN32
        if(!MoveFileExW(temporary.c_str(),result.cache.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("cannot commit runtime DSF cache");
#else
        std::filesystem::rename(temporary,result.cache);
#endif
    } catch(...) {std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}
    result.note="Building source: extracted runtime scenery DSF: "+std::to_string(result.buildings)+" footprints ("+std::to_string(result.terminals)+" terminals), "+std::to_string(result.tiles)+" tiles";
    if(!errors.empty())result.note+="; skipped unreadable tiles: "+errors;
    return result;
}
}
