#pragma once
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace oans::lr {
enum class Model { other, laminar, calibra };

// Calibra retains the original studio tag and adds its credit to the author field.
inline Model identify(const std::filesystem::path& model) {
    std::ifstream file(model);
    std::string line,studio,icao,author;
    while(std::getline(file,line)) {
        std::istringstream row(line);
        std::string tag,key;
        if(!(row>>tag))continue;
        if(tag=="PANEL_2D_BEGIN"||tag=="PANEL_3D_BEGIN")break;
        if(tag!="P"||!(row>>key))continue;
        auto* field=key=="acf/_studio"?&studio:key=="acf/_ICAO"?&icao:key=="acf/_author"?&author:nullptr;
        if(!field)continue;
        std::getline(row>>std::ws,*field);
        const auto end=field->find_last_not_of(" \t\r");
        if(end!=std::string::npos)field->erase(end+1);
        if(!studio.empty()&&!icao.empty()&&!author.empty())break;
    }
    if(studio!="Laminar Research")return Model::other;
    if(icao=="A332"&&author.find("Calibra Simulations")!=std::string::npos)return Model::calibra;
    return Model::laminar;
}
}
