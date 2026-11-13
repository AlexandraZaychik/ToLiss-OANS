#include "backend.hpp"
#include "identity.hpp"
#include "aircraft_profile.hpp"
#include <XPLMPlanes.h>
#include <algorithm>
#include <cmath>

namespace oans::magknight {
namespace {
using aircraft::DataAccess;
std::string active_model;
bool matches() {
    char name[512]{},path[2048]{}; XPLMGetNthAircraftModel(0,name,path);
    const auto model=std::filesystem::u8path(path);
    std::error_code error; const auto modified=std::filesystem::last_write_time(model,error);
    if(error)return false;
    static std::filesystem::path cached; static std::filesystem::file_time_type stamp{}; static bool result=false;
    if(model!=cached||modified!=stamp) {result=!identify(model).empty();cached=model;stamp=modified;}
    return result;
}
bool ready(const DataAccess& d) {
    return d.numeric("range0",0)&&d.numeric("mode0",0)&&d.numeric("mk_nav",0)&&
        d.numeric("mk_irs",0)&&d.numeric("mk_brightness",0);
}
bool load_profile(const std::filesystem::path& directory,const std::filesystem::path& model,
                  const std::string&,std::filesystem::path& loaded,std::string& error) {
    loaded.clear(); active_model.clear();
    const auto id=identify(model);
    if(id.empty()) {::profile::current={};error="Unsupported Magknight aircraft identity";return false;}
    if(!::profile::load(directory,id,error))return false;
    active_model=id;
    auto filename=id; std::transform(filename.begin(),filename.end(),filename.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c));});
    loaded=directory/(filename+".json"); return true;
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
    if(side!=0)return "MAGKNIGHT 787 FO ND NOT YET ADAPTED";
    if(!ready(d))return "MAGKNIGHT 787 DISPLAY INTERFACE UNAVAILABLE";
    if(d.value("mk_nav",0,0)!=1&&d.value("mk_nav",1,0)!=1)return "MAGKNIGHT 787 NAV DISPLAY NOT ACTIVE";
    if(!position(d,side))return "MAGKNIGHT 787 DISPLAY OFF OR IRS UNALIGNED";
    if(mode(d,side)<0)return "SELECT MAP / PLAN";
    const double range=d.value("range0",0,-1);
    if(!std::isfinite(range)||range<=0||range>5)return "SET PHYSICAL ND RANGE TO 5 NM OR LESS";
    return "";
}
bool advisory(const DataAccess&,int,bool) {return false;}
const double* uv(const DataAccess&,int side,bool pfd) {
    return pfd ? ::profile::current.pfd_uv[0][side] : ::profile::current.nd_uv[0][side];
}
void diagnostics(std::ostream& out,const DataAccess& d) {
    out<<"MAGKNIGHT_"<<(active_model.empty()?"UNKNOWN":active_model)<<" experimental=1 captain_only=1 fms=unavailable nav="<<d.value("mk_nav",0,-1)
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
