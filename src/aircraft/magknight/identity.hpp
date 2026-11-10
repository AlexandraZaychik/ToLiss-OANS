#pragma once
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace oans::magknight {
// The 787-10 in this package declares B789, so neither ICAO nor filename
// alone is sufficient. Restrict this first adapter to the inspected 787-8.
inline bool matches_b788(const std::filesystem::path& model) {
    std::ifstream file(model);
    if (!file) return false;
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
    return !file.bad() && icao == "B788" &&
        author.find("Magknight") != std::string::npos &&
        name.find("Magknight 787-8 ") == 0;
}
}
