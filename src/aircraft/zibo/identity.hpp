#pragma once
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
namespace oans::zibo {
inline bool matches_model(const std::filesystem::path& model) {
    std::ifstream file(model);std::string line,studio,icao;
    while(std::getline(file,line)) {
        if(line=="PANEL_2D_BEGIN"||line=="PANEL_3D_BEGIN")break;
        std::istringstream row(line);std::string tag,key,value;
        if(!(row>>tag>>key)||tag!="P")continue;
        std::getline(row>>std::ws,value);
        const auto end=value.find_last_not_of(" \t\r");
        value=end==std::string::npos?std::string{}:value.substr(0,end+1);
        if(key=="acf/_studio")studio=value;
        if(key=="acf/_ICAO")icao=value;
    }
    return icao=="B738"&&studio=="Laminar Research modified by Zibo and Twkster";
}
}
