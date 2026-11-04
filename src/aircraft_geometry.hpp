#pragma once
#include <limits>
#include <string_view>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>

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
// LR A330-300: supplied Air China A330/A340 FCOM, ALL 330-343,
// DSC-20-20-00000766.0005001 / 21 MAR 16 (metres).
inline constexpr Dimensions a333{63.69, 60.30, 6.67};
// Zibo B738: supplied drawing, overall length including tail extremity and span.
// Nose-to-nose-gear is approximately 4.4 m, scaled from the drawing (not labelled).
inline constexpr Dimensions b738{39.50, 34.40, 4.40};
// A339 drawing provides overall length only; span and nose-to-gear distance
// need additional dimensions before this entry can be used for protection.
inline constexpr double unknown_dimension = std::numeric_limits<double>::quiet_NaN();
inline constexpr Dimensions a339{63.69, unknown_dimension, unknown_dimension};
inline const Dimensions* dimensions_for(std::string_view family, std::string_view model) {
    std::string icao(model);
    for(auto& c:icao)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (family == "lr") return icao == "A333" ? &a333 : nullptr;
    if (family == "zibo") return icao == "B738" ? &b738 : nullptr;
    if (family != "toliss") return nullptr;
    if (icao == "A319" || icao == "A19N") return &a319_a19n;
    if (icao == "A320" || icao == "A20N") return &a320_a20n;
    if (icao == "A321" || icao == "A21N") return &a321_a21n;
    if (icao == "A346") return &a346;
    if (icao == "A339") return &a339;
    return nullptr;
}

// Final fallback: the original calibrated ToLiss A320/A20N envelope, in metres.
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

struct Envelope {
    double nose, tail, half_span;
    bool fallback;
};
inline constexpr Envelope default_envelope{nose,tail,half_span,true};
// Dimensions and longitudinal reference calibration must both be available.
// Never borrow another vendor's model entry or mix its reference with these dimensions.
inline Envelope envelope_for(std::string_view family,std::string_view icao,
                             double reference,double nose_gear) {
    const auto* d=dimensions_for(family,icao);
    if(!d||!std::isfinite(d->fuselage_length)||!std::isfinite(d->span)||
       !std::isfinite(d->nose_to_nose_gear)||!std::isfinite(reference)||!std::isfinite(nose_gear)||
       d->fuselage_length<=0||d->span<=0||d->nose_to_nose_gear<=0)return default_envelope;
    const double forward=(reference-nose_gear)*.3048+d->nose_to_nose_gear;
    if(forward<=0||forward>=d->fuselage_length)return default_envelope;
    return {forward,d->fuselage_length-forward,d->span*.5,false};
}
// Use the active ACF's model origin and nose gear, rather than A320 calibration.
inline Envelope load_envelope(std::string_view family,std::string_view icao,
                              const std::filesystem::path& acf) {
    std::ifstream file(acf);
    double reference=unknown_dimension,gear=unknown_dimension;
    std::string line;
    while(std::getline(file,line)) {
        std::istringstream row(line);std::string prefix,key;double value;
        if(!(row>>prefix>>key>>value)||prefix!="P")continue;
        if(key=="acf/_cgZ")reference=value;
        else if(key=="_gear/0/_gear_z")gear=value;
    }
    if(file.bad())return default_envelope;
    return envelope_for(family,icao,reference,gear);
}
}
