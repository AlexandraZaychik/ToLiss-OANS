#pragma once
#include "aircraft/backend.hpp"
#include "aircraft_profile.hpp"
namespace oans::zibo::display {
using aircraft::DataAccess;
inline std::string key(const char* name,int side) {return std::string(name)+std::to_string(side);}
inline bool ready(const DataAccess& d) {
    for(int side=0;side<2;++side) {
        for(const char* name:{"mode","range","expanded","vsd","du","irs"})
            if(!d.numeric(key(name,side),0))return false;
        if(!d.numeric("bright",::profile::current.nd_brightness[side]))return false;
    }
    return true;
}
inline bool display_on(const DataAccess& d,int side) {return d.value(key("du",side),0,-1)==0;}
inline bool powered(const DataAccess& d) {return display_on(d,0)||display_on(d,1);}
inline int nd_mode(const DataAccess& d,int side) {
    const auto mode=d.value(key("mode",side),0,-1);
    // Boeing APP=0, VOR=1, MAP=2, PLN=3; expanded MAP maps to ARC.
    if(mode==2)return d.value(key("expanded",side),0,0)==1?3:2;
    return mode==3?4:-1;
}
inline bool position_ready(const DataAccess& d,int side) {
    if(side<0)return position_ready(d,0)||position_ready(d,1);
    return display_on(d,side)&&d.value(key("irs",side),0,-1)==0;
}
inline double brightness(const DataAccess& d,int side,bool pfd) {
    return d.value("bright",pfd?::profile::current.pfd_brightness[side]: ::profile::current.nd_brightness[side],0);
}
inline bool advisory_available(const DataAccess& d,int side,bool pfd) {
    // Normal outboard PFD / inboard ND only. Keep warnings independent of
    // the OANS map's physical range and of the other display's power/brightness.
    const auto controller=key(pfd?"pfd_du":"du",side);
    if(!d.numeric(controller,0)||d.value(controller,0,-1)!=0)return false;
    if(!pfd) {
        const auto mode=key("mode",side),vsd=key("vsd",side);
        if(!d.numeric(mode,0)||nd_mode(d,side)<2||
           !d.numeric(vsd,0)||d.value(vsd,0,-1)!=0)return false;
    }
    const int index=pfd?::profile::current.pfd_brightness[side]: ::profile::current.nd_brightness[side];
    return d.numeric("bright",index)&&brightness(d,side,pfd)>=.01;
}
inline const char* overlay_blocked(const DataAccess& d,int side,bool) {
    if(!display_on(d,side))return "ZIBO ND OFF / DISPLAY SWITCHING ACTIVE";
    // Zibo's physical selector uses indices, with 0=5 NM and 1=10 NM.
    if(d.value(key("range",side),0,-1)!=0)return "SET PHYSICAL ND RANGE TO 5 NM";
    if(nd_mode(d,side)<2)return "SELECT MAP / PLN";
    if(d.value(key("vsd",side),0,0)!=0)return "DISABLE VSD";
    if(brightness(d,side,false)<.01)return "ND BRIGHTNESS OFF";
    if(!position_ready(d,side))return "ZIBO INERTIAL POSITION UNAVAILABLE";
    return "";
}
}
