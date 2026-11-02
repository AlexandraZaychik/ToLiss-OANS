#pragma once
#include "aircraft/backend.hpp"
#include "aircraft_profile.hpp"

namespace oans::lr::display {
using aircraft::DataAccess;
inline std::string key(const char* name,int side) {return std::string(name)+std::to_string(side);}
inline bool ready(const DataAccess& d) {
    for(int side=0;side<2;++side) {
        for(const char* name:{"mode","range","nd_state","map","att","hdg","lrn","swap","ecam"})
            if(!d.numeric(key(name,side),0))return false;
        if(!d.numeric("bright",::profile::current.nd_brightness[side]))return false;
    }
    return true;
}
inline bool display_on(const DataAccess& d,int side) {
    // LR sys_displays: 0=off, 1=warmup, 2=self-test, 3=graphics display.
    return d.value(key("nd_state",side),0,0)==3;
}
inline bool powered(const DataAccess& d) {return display_on(d,0)||display_on(d,1);}
inline int nd_mode(const DataAccess& d,int side) {
    // Read the physical Airbus selector: LS=0, VOR=1, NAV=2, ARC=3, PLAN=4, ENG=5.
    // The standard X-Plane map_mode has different NAV/ARC codes.
    return static_cast<int>(d.value(key("mode",side),0,-1));
}
inline bool position_side_ready(const DataAccess& d,int side) {
    if(!display_on(d,side))return false;
    for(const char* name:{"att","hdg","lrn"})if(d.value(key(name,side),0,0)!=1)return false;
    return true;
}
inline bool position_ready(const DataAccess& d,int side) {
    return side<0 ? position_side_ready(d,0)||position_side_ready(d,1) : position_side_ready(d,side);
}
inline double brightness(const DataAccess& d,int side,bool pfd) {
    const int index=pfd ? ::profile::current.pfd_brightness[side] : ::profile::current.nd_brightness[side];
    return d.value("bright",index,0);
}
inline const char* overlay_blocked(const DataAccess& d,int side,bool test) {
    if(!display_on(d,side))return "LR ND OFF / WARMUP / SELF TEST";
    if(std::abs(d.value(key("range",side),0,-1)-10)>.01)return "SET PHYSICAL ND RANGE TO 10";
    const int mode=nd_mode(d,side);
    if(!test&&(mode<2||mode>4))return "SELECT NAV / ARC / PLAN";
    if(d.value(key("swap",side),0,0)!=0||d.value(key("ecam",side),0,0)!=0)return "DISPLAY SWITCHING ACTIVE";
    if(brightness(d,side,false)<.01)return "ND BRIGHTNESS OFF";
    if(d.value(key("map",side),0,0)!=2||!position_side_ready(d,side))return "LR INERTIAL POSITION UNAVAILABLE";
    return "";
}
}
