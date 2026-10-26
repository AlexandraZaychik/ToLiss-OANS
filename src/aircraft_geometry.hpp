#pragma once
#include <limits>
#include <string_view>

namespace oans::aircraft_geometry {
struct Dimensions {
    double fuselage_length; // metres
    double span; // metres
    double nose_to_nose_gear; // metres
};
// Shared dimensions by ICAO family. A319/A19N, A321/A21N and A346 values
// follow the supplied drawings. Reference-point calibration is separate from dimensions.
inline constexpr Dimensions a319_a19n{33.84, 34.10, 5.07};
inline constexpr Dimensions a320_a20n{37.57, 35.80, 5.07};
inline constexpr Dimensions a321_a21n{44.51, 35.80, 5.07};
inline constexpr Dimensions a346{75.27, 63.45, 6.57};
// A339 drawing provides overall length only; span and nose-to-gear distance
// need additional dimensions before this entry can be used for protection.
inline constexpr double unknown_dimension = std::numeric_limits<double>::quiet_NaN();
inline constexpr Dimensions a339{63.69, unknown_dimension, unknown_dimension};
inline constexpr const Dimensions* dimensions_for(std::string_view icao) {
    if (icao == "A319" || icao == "A19N") return &a319_a19n;
    if (icao == "A320" || icao == "A20N") return &a320_a20n;
    if (icao == "A321" || icao == "A21N") return &a321_a21n;
    if (icao == "A346") return &a346;
    if (icao == "A339") return &a339;
    return nullptr;
}

// Shared A320/A20N protection envelope used by RWY AHEAD, in metres.
// Manual dimensions; these are not loaded from the active aircraft yet.
inline constexpr double fuselage_length = a320_a20n.fuselage_length;
inline constexpr double span = a320_a20n.span;

// Installed ACF positions in feet, measured along its longitudinal Z axis.
inline constexpr double reference_z_ft = 54.0;
inline constexpr double nose_gear_z_ft = 17.729999542;
inline constexpr double nose_to_nose_gear = a320_a20n.nose_to_nose_gear;

// Distances forward/aft of the simulator position reference, not the
// geometric fuselage centre. Preserve the original calibration formula.
inline constexpr double nose = (reference_z_ft - nose_gear_z_ft) * 0.3048 + nose_to_nose_gear;
inline constexpr double tail = fuselage_length - nose;
inline constexpr double half_span = span / 2;
}
