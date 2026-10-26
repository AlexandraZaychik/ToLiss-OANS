#pragma once
#include <string_view>

namespace oans::aircraft_limits {
struct RunwayLimits {
    std::string_view icao;
    double minimum_length; // metres
    double minimum_width; // metres
};

// Airport eligibility: one runway must meet both limits.
// Unlisted aircraft have no additional runway size restriction.
inline constexpr RunwayLimits runway_limits[] = {
    {"A319", 1200, 30},
    {"A19N", 1200, 30},
    {"A320", 1200, 30},
    {"A20N", 1200, 30},
    {"A321", 1200, 30},
    {"A21N", 1200, 30},
    {"A339", 1600, 45},
    {"A346", 1700, 45},
};

// The caller normalizes ICAO to uppercase.
inline constexpr RunwayLimits runway_limits_for(std::string_view icao) {
    for (const auto& limits : runway_limits)
        if (limits.icao == icao) return limits;
    return {icao, 0, 0};
}
}
