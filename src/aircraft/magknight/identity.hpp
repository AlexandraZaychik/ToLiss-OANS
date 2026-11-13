#pragma once
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace oans::magknight {
// The inspected 787-10 declares B789. Use the authored model name as well as
// vendor and ICAO; filenames and the simulator's ICAO cannot select its profile.
inline std::string identify(const std::filesystem::path& model) {
    std::ifstream file(model);
    if (!file) return {};
    std::string line, author, name, icao;
    while (std::getline(file, line)) {
        if (line == "PANEL_2D_BEGIN" || line == "PANEL_3D_BEGIN") break;
        std::istringstream row(line);
        std::string prefix, key, value;
        if (!(row >> prefix >> key) || prefix != "P") continue;
        std::getline(row >> std::ws, value);
        const auto end = value.find_last_not_of(" \t\r");
        value = end == std::string::npos ? std::string{} : value.substr(0, end + 1);
        if (key == "acf/_author") author = value;
        else if (key == "acf/_name") name = value;
        else if (key == "acf/_ICAO") icao = value;
    }
    if (file.bad() || author.find("Magknight") == std::string::npos) return {};
    if (icao == "B788" && name.find("Magknight 787-8 ") == 0) return "B788";
    if (icao == "B789" && name.find("Magknight 787-9 ") == 0) return "B789";
    if ((icao == "B789" || icao == "B78X") && name.find("Magknight 787-10 ") == 0) return "B78X";
    return {};
}
}
