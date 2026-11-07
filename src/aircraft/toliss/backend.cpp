// Expose SDK 4 registry declarations; resolve them dynamically to retain
// compatibility with hosts where enumeration is unavailable.
#ifndef XPLM400
#define XPLM400 1
#endif
#include "backend.hpp"
#include "profile.hpp"
#include "mcdu_xplm.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace oans::toliss {
namespace {
using aircraft::DataAccess;
bool contains_toliss(std::string name) {
    std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return name.find("toliss")!=std::string::npos;
}
bool matches() {
    // Identity and interface readiness are separate: providers may load later.
    // Match registered names, with no fixed DataRef list or value requirement.
    static const auto count_refs=reinterpret_cast<decltype(&XPLMCountDataRefs)>(XPLMFindSymbol("XPLMCountDataRefs"));
    static const auto list_refs=reinterpret_cast<decltype(&XPLMGetDataRefsByIndex)>(XPLMFindSymbol("XPLMGetDataRefsByIndex"));
    static const auto ref_info=reinterpret_cast<decltype(&XPLMGetDataRefInfo)>(XPLMFindSymbol("XPLMGetDataRefInfo"));
    if(count_refs&&list_refs&&ref_info) {
        std::array<XPLMDataRef,128> refs{};
        const int total=count_refs();
        for(int offset=0;offset<total;offset+=static_cast<int>(refs.size())) {
            const int count=std::min(static_cast<int>(refs.size()),total-offset);
            list_refs(offset,count,refs.data());
            for(int i=0;i<count;++i)if(refs[i]) {
                XPLMDataRefInfo_t info{};info.structSize=sizeof(info);
                ref_info(refs[i],&info);
                if(info.name&&contains_toliss(info.name))return true;
            }
        }
    }
    auto ref=XPLMFindDataRef("sim/aircraft/view/acf_ui_name");
    if(!ref||!(XPLMGetDataRefTypes(ref)&xplmType_Data))return false;
    std::array<char,1024> buffer{};
    const int count=XPLMGetDatab(ref,buffer.data(),0,static_cast<int>(buffer.size()-1));
    if(count<=0)return false;
    std::string name(buffer.data(),std::find(buffer.data(),buffer.data()+std::min(count,static_cast<int>(buffer.size()-1)),'\0'));
    return contains_toliss(name);
}
bool powered(const DataAccess& d) {
    return d.numeric("bus",1)&&std::max(d.value("bus",0,0),d.value("bus",1,0))>=20;
}
bool ready(const DataAccess& d) {return d.numeric("range0",0)&&d.numeric("mode0",0);}
void diagnostics(std::ostream& out,const DataAccess& d) {
    out<<"OANS_IRS status_present="<<d.numeric("irs_align",0)<<" seconds_remaining="<<d.value("irs_align",0,0)<<'\n';
}
int nd_mode(const DataAccess& d,int side) {
    return static_cast<int>(d.value("mode"+std::to_string(side),0,-1));
}
bool position_ready(const DataAccess& d,int side) {
    const double align=d.value("irs_align",0,0);
    auto map=[&](int i){const auto key="map"+std::to_string(i);return d.numeric(key,0)&&d.value(key,0,0)>=.5;};
    return d.numeric("irs_align",0)&&std::isfinite(align)&&align<=0&&powered(d)&&
        ((side!=1&&map(0))||(side!=0&&map(1)));
}
double brightness(const DataAccess& d,int side,bool pfd) {
    const int index=pfd?::profile::current.pfd_brightness[side] : ::profile::current.nd_brightness[side];
    return d.value("bright",index,0);
}
bool switched(const DataAccess& d,int side) {
    return d.value("swap"+std::to_string(side),0,0)!=0||d.value("ecamSwap",0,0)!=0;
}
const char* overlay_blocked(const DataAccess& d,int side,bool test) {
    if(d.value("range"+std::to_string(side),0,99)!=0)return "SET PHYSICAL ND RANGE TO 10";
    const int mode=nd_mode(d,side);
    if(!test&&(mode<2||mode>4))return "SELECT NAV / ARC / PLAN";
    if(d.value("map"+std::to_string(side),0,0)<.5)return "AIRCRAFT MAP UNAVAILABLE";
    if(brightness(d,side,false)<.01)return "ND BRIGHTNESS OFF";
    if(switched(d,side))return "DISPLAY SWITCHING ACTIVE";
    // Match the existing overlay policy: an absent bus does not add a block.
    if(d.numeric("bus",1)&&!powered(d))return "DISPLAY BUS BELOW 20V";
    return "";
}
bool advisory_available(const DataAccess& d,int side,bool pfd) {
    if(switched(d,side))return false;
    const int mode=nd_mode(d,side);
    if(!pfd&&(mode<2||mode>4))return false;
    const int index=pfd?::profile::current.pfd_brightness[side] : ::profile::current.nd_brightness[side];
    return d.numeric("bright",index)&&brightness(d,side,pfd)>=.01&&powered(d);
}
const double* uv(const DataAccess& d,int side,bool pfd) {
    // Older ToLiss providers omit this interface; use the normal layout then.
    const int resolution=d.numeric("fallback",0)&&d.value("fallback",0,0)>.5?1:0;
    return pfd?::profile::current.pfd_uv[resolution][side] : ::profile::current.nd_uv[resolution][side];
}
std::unique_ptr<FmsSource> make_fms() {return std::make_unique<McduFmsPort>();}
}
const aircraft::Backend& backend() {
    static const aircraft::Backend value{
        "toliss","ToLiss",{
            {"range0","AirbusFBW/NDrangeCapt"},{"range1","AirbusFBW/NDrangeFO"},
            {"mode0","AirbusFBW/NDmodeCapt"},{"mode1","AirbusFBW/NDmodeFO"},
            {"map0","AirbusFBW/CaptMAPAvail"},{"map1","AirbusFBW/CoMAPAvail"},
            {"irs_align","AirbusFBW/TimeToAlign"},
            {"plan_phase","AirbusFBW/ECAMFlightPhase"},
            {"bright","AirbusFBW/DUBrightness"},{"fallback","AirbusFBW/DisplayResolutionFallback"},
            {"swap0","AirbusFBW/PFDNDToggleCapt"},{"swap1","AirbusFBW/PFDNDToggleCo"},
            {"ecamSwap","AirbusFBW/ECAMNDSwitching"}
        },matches,ready,diagnostics,load_for_aircraft,make_fms,nd_mode,position_ready,powered,
        overlay_blocked,advisory_available,uv,brightness
    };
    return value;
}
}
