#include "backend.hpp"
#include "identity.hpp"
#include "display.hpp"
#include "mcdu_xplm.hpp"
#include "aircraft_profile.hpp"
#include <XPLMPlanes.h>

namespace oans::lr {
namespace {
using aircraft::DataAccess;
bool matches() {
    char name[512]{},path[2048]{};XPLMGetNthAircraftModel(0,name,path);
    const auto model=std::filesystem::u8path(path);
    std::error_code error;
    const auto modified=std::filesystem::last_write_time(model,error);
    if(error)return false;
    // Probe on the flight loop without reparsing the ACF on every retry.
    static std::filesystem::path cached_model;
    static std::filesystem::file_time_type cached_modified{};
    static bool cached_match=false;
    if(model!=cached_model||modified!=cached_modified) {
        cached_match=matches_model(model);cached_model=model;cached_modified=modified;
    }
    return cached_match;
}
bool load_profile(const std::filesystem::path& directory,const std::filesystem::path&,
                  const std::string& icao,std::filesystem::path& loaded,std::string& error) {
    loaded.clear();
    if(!::profile::load(directory,icao,error))return false;
    auto filename=icao;
    std::transform(filename.begin(),filename.end(),filename.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    loaded=directory/(filename+".json");return true;
}
void diagnostics(std::ostream& out,const DataAccess& d) {
    out<<"LR_ADAPTER display_ready="<<display::ready(d)<<" fms_source=LR_MCDU2\n";
    for(int side=0;side<2;++side)
        out<<"LR_ND side="<<side<<" mode="<<display::nd_mode(d,side)
           <<" range_nm="<<d.value(display::key("range",side),0,-1)
           <<" display_state="<<d.value(display::key("nd_state",side),0,-1)
           <<" position_ready="<<display::position_ready(d,side)
           <<" brightness="<<display::brightness(d,side,false)<<'\n';
}
std::unique_ptr<FmsSource> make_fms() {return std::make_unique<McduFmsPort>(); }
const double* uv(const DataAccess&,int side,bool pfd) {
    return pfd ? ::profile::current.pfd_uv[0][side] : ::profile::current.nd_uv[0][side];
}
}
const aircraft::Backend& backend() {
    static const aircraft::Backend value{
        "lr","Laminar Research",{
            {"plan_phase","laminar/A333/data/flight_phase"},
            {"range0","sim/cockpit2/EFIS/map_range_nm"},{"range1","sim/cockpit2/EFIS/map_range_nm_copilot"},
            {"mode0","laminar/A333/knobs/EFIS_mode_pos_capt"},{"mode1","laminar/A333/knobs/EFIS_mode_pos_fo"},
            {"bright","sim/cockpit2/switches/instrument_brightness_ratio"},
            {"nd_state0","laminar/A333/nd1/display_state"},{"nd_state1","laminar/A333/nd2/display_state"},
            {"pfd_state0","laminar/A333/pfd1/display_state"},{"pfd_state1","laminar/A333/pfd2/display_state"},
            {"map0","laminar/A333/ND/draw_map_mode_capt"},{"map1","laminar/A333/ND/draw_map_mode_fo"},
            {"att0","laminar/A333/adiru1/att_status"},{"att1","laminar/A333/adiru2/att_status"},
            {"hdg0","laminar/A333/adiru1/hdg_status"},{"hdg1","laminar/A333/adiru2/hdg_status"},
            {"lrn0","laminar/A333/adiru1/lrn_status"},{"lrn1","laminar/A333/adiru2/lrn_status"},
            {"swap0","laminar/A333/buttons/pfd_nd_swap_capt_status"},{"swap1","laminar/A333/buttons/pfd_nd_swap_fo_status"},
            {"ecam0","laminar/A333/ECAM_on_EFIS_capt"},{"ecam1","laminar/A333/ECAM_on_EFIS_fo"}
        },matches,display::ready,diagnostics,load_profile,make_fms,display::nd_mode,
        display::position_ready,display::powered,display::overlay_blocked,display::advisory_available,uv,display::brightness
    };
    return value;
}
}
