#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <iterator>
#include <string>
#include <vector>

namespace profile {
struct AircraftProfile {
    int nd_brightness[2]{}, pfd_brightness[2]{};
    double nd_uv[2][2][4]{}, pfd_uv[2][2][4]{}, nd_corners[2][4][3]{};
};
inline AircraftProfile current;

// Strict reader for this numeric-array JSON schema; no external JSON dependency.
class JsonReader {
    std::string text;
    std::size_t pos=0;
public:
    explicit JsonReader(std::string value):text(std::move(value)) {}
    void whitespace() {while(pos<text.size()&&std::isspace(static_cast<unsigned char>(text[pos])))++pos;}
    bool take(char c) {whitespace();if(pos==text.size()||text[pos]!=c)return false;++pos;return true;}
    bool done() {whitespace();return pos==text.size();}
    bool key(std::string& value) {
        if(!take('"'))return false;
        value.clear();
        while(pos<text.size()&&text[pos]!='"') {
            const char c=text[pos++];
            if(!((c>='a'&&c<='z')||c=='_'))return false;
            value+=c;
        }
        return take('"')&&take(':');
    }
    bool number(double& value) {
        whitespace();const auto start=pos;
        while(pos<text.size()&&std::string("-+0123456789.eE").find(text[pos])!=std::string::npos)++pos;
        const auto token=text.substr(start,pos-start);
        static const std::regex syntax(R"(-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?)");
        if(!std::regex_match(token,syntax))return false;
        try {value=std::stod(token);return std::isfinite(value);}catch(...) {return false;}
    }
    bool array(const std::vector<int>& shape, std::size_t depth, std::vector<double>& values) {
        if(!take('['))return false;
        for(int i=0;i<shape[depth];++i) {
            if(i&&!take(','))return false;
            if(depth+1<shape.size()) {if(!array(shape,depth+1,values))return false;}
            else {double value;if(!number(value))return false;values.push_back(value);}
        }
        return take(']');
    }
};
inline bool load_file(const std::filesystem::path& path, std::string& error) {
    current={};error.clear();
    std::ifstream file(path);
    if(!file) {error="Cannot open aircraft profile: "+path.u8string();return false;}
    JsonReader input(std::string((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>()));
    AircraftProfile candidate{};
    bool seen[5]{};
    auto fail=[&](const std::string& reason){error=path.u8string()+": "+reason;return false;};
    if(file.bad())return fail("Profile read failed");
    if(!input.take('{'))return fail("Expected JSON object");
    bool first=true;
    while(!input.take('}')) {
        if(!first&&!input.take(','))return fail("Expected comma between fields");
        first=false;std::string key;
        if(!input.key(key))return fail("Invalid JSON field");
        const char* keys[]={"nd_brightness","pfd_brightness","nd_uv","pfd_uv","nd_corners"};
        int index=0;while(index<5&&key!=keys[index])++index;
        if(index==5)return fail("Unknown field: "+key);
        if(seen[index])return fail("Duplicate field: "+key);
        const std::vector<int> shapes[]={{2},{2},{2,2,4},{2,2,4},{2,4,3}};
        std::vector<double> parsed;
        if(!input.array(shapes[index],0,parsed))return fail("Invalid numeric array/shape: "+key);
        if(index<2) {
            for(int side=0;side<2;++side) {
                const double v=parsed[side];
                if(v<0||v>255||std::floor(v)!=v)return fail("Invalid brightness index: "+key);
                (index==0?candidate.nd_brightness:candidate.pfd_brightness)[side]=static_cast<int>(v);
            }
        } else if(index<4) {
            for(int resolution=0;resolution<2;++resolution)for(int side=0;side<2;++side) {
                auto& uv=index==2?candidate.nd_uv[resolution][side]:candidate.pfd_uv[resolution][side];
                for(int j=0;j<4;++j) {uv[j]=parsed[(resolution*2+side)*4+j];if(uv[j]<0||uv[j]>1)return fail("UV outside [0,1]: "+key);}
                if(uv[0]>=uv[2]||uv[1]>=uv[3])return fail("Empty/reversed UV rectangle: "+key);
            }
        } else {
            for(int side=0;side<2;++side)for(int corner=0;corner<4;++corner)for(int axis=0;axis<3;++axis)
                candidate.nd_corners[side][corner][axis]=parsed[(side*4+corner)*3+axis];
        }
        seen[index]=true;
    }
    if(!input.done())return fail("Unexpected content after JSON object");
    for(int i=0;i<5;++i)if(!seen[i])return fail("Missing required profile field");
    current=candidate;return true;
}
inline bool valid_icao(const std::string& icao) {
    return !icao.empty()&&icao.size()<=4&&std::all_of(icao.begin(),icao.end(),[](unsigned char c){
        return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9');
    });
}
inline bool load(const std::filesystem::path& directory, std::string icao, std::string& error) {
    if(!valid_icao(icao)) {current={};error="Invalid aircraft ICAO: "+icao;return false;}
    std::transform(icao.begin(),icao.end(),icao.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return load_file(directory/(icao+".json"),error);
}
}
