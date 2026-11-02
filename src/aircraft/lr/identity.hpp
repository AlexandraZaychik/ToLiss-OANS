#pragma once
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace oans::lr {
// Studio identifies the family; neither the aircraft folder nor ICAO identifies it.
inline bool matches_model(const std::filesystem::path& model) {
    std::ifstream file(model);
    std::string line;
    while(std::getline(file,line)) {
        if(line=="PANEL_2D_BEGIN"||line=="PANEL_3D_BEGIN")break;
        std::istringstream row(line);std::string tag,key,studio;
        if(row>>tag>>key&&tag=="P"&&key=="acf/_studio") {
            std::getline(row>>std::ws,studio);
            const auto end=studio.find_last_not_of(" \t\r");
            studio=end==std::string::npos?std::string{}:studio.substr(0,end+1);
            return studio=="Laminar Research";
        }
    }
    return false;
}
}
