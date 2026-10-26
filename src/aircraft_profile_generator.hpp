#pragma once
#include <filesystem>
#include <string>

namespace profile {
// Active ACF stem + first four lowercase hex digits of ACF MD5 and cockpit OBJ MD5.
// Resolve only the active ACF's attachments: prefer cockpit+inn, then unique cockpit.
bool identify(const std::filesystem::path& model, std::string& id, std::string& error);
// Load <id>.json, then distributed <lowercase ICAO>.json, else generate/load <id>.json.
// Existing invalid files fail; base profiles are never generated or overwritten.
bool load_for_aircraft(const std::filesystem::path& directory,
                       const std::filesystem::path& model, const std::string& icao,
                       std::filesystem::path& loaded, std::string& error);
}
