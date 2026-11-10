#pragma once
#include "fms_light_capture.hpp"
#include <XPLMDataAccess.h>
#include <array>
#include <cmath>

namespace oans {
// Switch states, not flashing brightness: a beacon pulse is not a new edge.
inline std::array<int,2> fms_light_switches() {
    std::array<int,2> states{{-1,-1}};
    if(auto ref=XPLMFindDataRef("sim/cockpit2/switches/beacon_on")) {
        if(XPLMGetDataRefTypes(ref)&xplmType_Int)states[0]=XPLMGetDatai(ref)!=0;
    }
    if(auto ref=XPLMFindDataRef("sim/cockpit2/switches/landing_lights_switch")) {
        if(XPLMGetDataRefTypes(ref)&xplmType_FloatArray) {
            // The aircraft's two landing lights occupy indices 0 and 1.
            float values[2]{};const int count=XPLMGetDatavf(ref,values,0,2);
            if(count==2)states[1]=fms_landing_pair_state(values[0],values[1]);
        }
    }
    return states;
}
}
