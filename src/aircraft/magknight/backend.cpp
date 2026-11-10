#include "backend.hpp"
#include "identity.hpp"
#include "aircraft_profile.hpp"
#include <XPLMPlanes.h>
#include <algorithm>
#include <cmath>

namespace oans::magknight {
namespace {
using aircraft::DataAccess;
bool matches() {
    char name[512]{},path[2048]{}; XPLMGetNthAircraftModel(0,name,path);
    const auto model=std::filesystem::u8path(path);
    std::error_code error; const auto modified=std::filesystem::last_write_time(model,error);
    if(error)return false;
    static std::filesystem::path cached; static std::filesystem::file_time_type stamp{}; static bool result=false;
    if(model!=cached||modified!=stamp) {result=matches_b788(model);cached=model;stamp=modified;}
    return result;
}
bool ready(const DataAccess& d) {
    return d.numeric("range0",0)&&d.numeric("mode0",0)&&d.numeric("mk_nav",0)&&
        d.numeric("mk_irs",0)&&d.numeric("mk_brightness",0);
}
bool load_profile(const std::filesystem::path& directory,const std::filesystem::path&,
                  const std::string&,std::filesystem::path& loaded,std::string& error) {
    loaded.clear(); if(!::profile::load(directory,"B788",error))return false;
    loaded=directory/"b788.json"; return true;
}
int mode(const DataAccess& d,int) {
    const double m=d.value("mode0",0,-1);
    if(m==2)return 3;
    if(m==4)return 4;
    return -1;
}
bool powered(const DataAccess& d) {return d.value("mk_brightness",0,0)>0;}
bool position(const DataAccess& d,int side) {
    return side!=1&&powered(d)&&d.value("mk_irs",0,0)==1;
}
double brightness(const DataAccess& d,int,bool) {
    return std::clamp(d.value("mk_brightness",0,0)/10.0,0.0,1.0);
}
const char* blocked(const DataAccess& d,int side,bool) {
    if(side!=0)return "B788 FO ND NOT YET ADAPTED";
    if(!ready(d))return "B788 DISPLAY INTERFACE UNAVAILABLE";
    if(d.value("mk_nav",0,0)!=1)return "B788 NAV DISPLAY NOT ACTIVE";
    if(!position(d,side))return "B788 DISPLAY OFF OR IRS UNALIGNED";
    if(mode(d,side)<0)return "SELECT MAP / PLAN";
    if(std::abs(d.value("range0",0,-1)-20)>0.01)return "SET PHYSICAL ND RANGE TO 20 NM";
    return "";
}
bool advisory(const DataAccess&,int,bool) {return false;}
const double* uv(const DataAccess&,int side,bool pfd) {
    return pfd ? ::profile::current.pfd_uv[0][side] : ::profile::current.nd_uv[0][side];
}
void diagnostics(std::ostream& out,const DataAccess& d) {
    out<<"MAGKNIGHT_B788 experimental=1 captain_only=1 fms=unavailable nav="<<d.value("mk_nav",0,-1)
       <<" irs="<<d.value("mk_irs",0,-1)<<" brightness="<<d.value("mk_brightness",0,-1)
       <<" mode="<<d.value("mode0",0,-1)<<" range="<<d.value("range0",0,-1)<<'\n';
}
std::unique_ptr<FmsSource> fms() {return nullptr;}
}
const aircraft::Backend& backend() {
    static const aircraft::Backend value{"magknight","Magknight",{
        {"range0","sim/cockpit2/EFIS/map_range_nm"}, {"mode0","sim/cockpit2/EFIS/map_mode"},
        {"mk_nav","aero787/cockpit/mfd/defaultNavDisplays"}, {"mk_irs","aero787/nav/irs/aligned"},
        {"mk_brightness","aero787/cockpit/lights/inbdDisplayL"}
    },matches,ready,diagnostics,load_profile,fms,mode,position,powered,blocked,advisory,uv,brightness};
    return value;
}
}
