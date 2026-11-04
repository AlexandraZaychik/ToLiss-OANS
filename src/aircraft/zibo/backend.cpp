#include "backend.hpp"
#include "identity.hpp"
#include "display.hpp"
#include "fms_xplm.hpp"
#include <XPLMPlanes.h>
namespace oans::zibo {
namespace {
using aircraft::DataAccess;
bool matches() {
    char name[512]{},path[2048]{};XPLMGetNthAircraftModel(0,name,path);
    const auto model=std::filesystem::u8path(path);
    std::error_code error;const auto modified=std::filesystem::last_write_time(model,error);
    if(error)return false;
    static std::filesystem::path cached_model;static std::filesystem::file_time_type cached_modified{};
    static bool cached_match=false;
    if(model!=cached_model||modified!=cached_modified) {
        cached_match=matches_model(model);cached_model=model;cached_modified=modified;
    }
    return cached_match;
}
bool load_profile(const std::filesystem::path& directory,const std::filesystem::path&,
                  const std::string& icao,std::filesystem::path& loaded,std::string& error) {
    loaded.clear();if(!::profile::load(directory,icao,error))return false;
    loaded=directory/"b738.json";return true;
}
void diagnostics(std::ostream& out,const DataAccess& d) {
    out<<"ZIBO_ADAPTER display_ready="<<display::ready(d)<<" physical_oans_range_nm=5 fms_source=ZIBO_DATAREF\n";
    for(int side=0;side<2;++side)
        out<<"ZIBO_ND side="<<side<<" mode="<<display::nd_mode(d,side)
           <<" range_index="<<d.value(display::key("range",side),0,-1)
           <<" display="<<d.value(display::key("du",side),0,-1)
           <<" position_ready="<<display::position_ready(d,side)
           <<" brightness="<<display::brightness(d,side,false)
           <<" pfd_display="<<d.value(display::key("pfd_du",side),0,-1)
           <<" advisory_nd="<<display::advisory_available(d,side,false)
           <<" advisory_pfd="<<display::advisory_available(d,side,true)
           <<" blocked=\""<<display::overlay_blocked(d,side,false)<<"\"\n";
}
std::unique_ptr<FmsSource> make_fms() {return std::make_unique<DatarefFmsPort>();}
const double* uv(const DataAccess&,int side,bool pfd) {
    return pfd?::profile::current.pfd_uv[0][side]: ::profile::current.nd_uv[0][side];
}
}
const aircraft::Backend& backend() {
    static const aircraft::Backend value{
        "zibo","Zibo",{
            {"range0","laminar/B738/EFIS/capt/map_range"},{"range1","laminar/B738/EFIS/fo/map_range"},
            {"mode0","laminar/B738/EFIS_control/capt/map_mode_pos"},{"mode1","laminar/B738/EFIS_control/fo/map_mode_pos"},
            {"expanded0","laminar/B738/EFIS_control/capt/exp_map"},{"expanded1","laminar/B738/EFIS_control/fo/exp_map"},
            {"vsd0","laminar/B738/EFIS_control/capt/vsd_map"},{"vsd1","laminar/B738/EFIS_control/fo/vsd_map"},
            {"du0","laminar/B738/display_units/idu_cpt"},{"du1","laminar/B738/display_units/idu_fo"},
            {"pfd_du0","laminar/B738/display_units/odu_cpt"},{"pfd_du1","laminar/B738/display_units/odu_fo"},
            {"irs0","laminar/B738/irs/nd_allign"},{"irs1","laminar/B738/irs/nd_allign2"},
            {"bright","sim/cockpit2/switches/instrument_brightness_ratio"}
        },matches,display::ready,diagnostics,load_profile,make_fms,display::nd_mode,
        display::position_ready,display::powered,display::overlay_blocked,display::advisory_available,uv,display::brightness
    };
    return value;
}
}
