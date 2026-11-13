#include "nd_ui.hpp"
#include "aircraft/magknight/range_control.hpp"
#include "map_viewport.hpp"
#include "test_support.hpp"
using namespace oans;
const UiControl& control(const UiFrame& f,UiAction action){for(const auto& c:f.controls)if(c.action==action)return c;throw std::runtime_error("missing UI control");}
int main(){try{
 scenario("ui/halved-range-ladder");
 UiState range_state;UiModel range_model;range_model.scale_button=true;
 verify(range_state.range_nm==2.5,"default range is 2.5 NM");
 range_state.range_nm=2.5;
 for(double expected:{2.5,1.,.5,.25,.1}) {
    verify(range_state.range_nm==expected,"zoom visits the new range ladder");
    const auto controls=nd_controls(range_state,range_model);
    verify(control(controls,UiAction::zoom_in).enabled==(expected>.1),"zoom-in bound");
    verify(control(controls,UiAction::zoom_out).enabled==(expected<2.5),"zoom-out bound");
    zoom(range_state,true);
 }
 verify(range_state.range_nm==.1&&!zoom(range_state,true),"minimum clamps at 0.1 NM");
 for(double expected:{.25,.5,1.,2.5}) {
    verify(zoom(range_state,false)&&range_state.range_nm==expected,"zoom-out visits the reversed ladder");
 }
 verify(!zoom(range_state,false)&&range_state.range_nm==2.5,"maximum clamps at 2.5 NM");
 scenario("ui/magknight-open-gate");
 for(double range:{.5,1.,2.5,5.})verify(magknight::can_open(true,true,range),"ground and range at or below 5 opens");
 for(double range:{0.,-1.,5.0001,10.,std::nan("")})verify(!magknight::can_open(true,true,range),"invalid or over-limit range cannot open");
 verify(!magknight::can_open(true,false,5),"airborne cannot open");
 verify(!magknight::can_open(false,true,5),"missing ground data cannot open");
 scenario("ui/magknight-ground-selector");
 magknight::RangeControl selector;
 verify(!selector.update(true,false,1,true).open,"airborne does not auto open");
 verify(selector.update(true,true,1,true).open,"ground 5 NM opens OANS");
 auto step=selector.update(true,true,0,true);
 verify(step.zoom_in&&step.reset_selector,"turn in zooms and resets selector");
 verify(!selector.update(true,true,0,false).zoom_in,"held selector does not repeat");
 selector.update(true,true,1,false);
 step=selector.update(true,true,2,false);
 verify(step.zoom_out&&step.reset_selector,"turn out zooms and resets selector");
 selector.update(true,true,1,true);
 step=selector.update(true,true,2,true);
 verify(step.close&&!step.reset_selector,"turn out at 5 exits at physical 10");
 verify(selector.update(true,true,1,true).open,"return to physical 5 opens OANS");
 verify(selector.update(true,false,1,true).close,"takeoff closes automatic OANS");
 selector.reset();verify(!selector.update(false,true,1,true).open,"missing data fails closed");
 scenario("ui/magknight-range-ladder");
 set_magknight_ranges(range_state,true);
 verify(range_state.range_nm==2,"787 converts the previous default to the nearest valid range");
 verify(zoom(range_state,false)&&range_state.range_nm==5,"787 maximum range");
 verify(!zoom(range_state,false)&&!control(nd_controls(range_state,range_model),UiAction::zoom_out).enabled,"787 maximum and button clamp");
 for(double expected:{2.,1.,.5})
    verify(zoom(range_state,true)&&range_state.range_nm==expected,"787 zoom-in visits each range");
 verify(!zoom(range_state,true)&&!control(nd_controls(range_state,range_model),UiAction::zoom_in).enabled,"787 minimum and button clamp");
 for(double expected:{1.,2.,5.})
    verify(zoom(range_state,false)&&range_state.range_nm==expected,"787 zoom-out visits each range");
 set_magknight_ranges(range_state,false);
 verify(range_state.range_nm==2.5&&!range_state.magknight_ranges,"switching aircraft restores the normal range ladder");
 scenario("ui/draft-confirmation-isolation");
 IndexEntry a;a.icao="HECA";a.iata="CAI";a.name="Cairo";IndexEntry b=a;b.icao="VHHH";b.iata="HKG";
 std::vector<IndexEntry> db={a,b};UiState s;s.panel=Panel::airport;UiModel model;
 select_airport_candidate(s,db,"HECA");verify(confirm_airport_candidate(s),"initial confirmation");
 begin_airport_modification(s);select_airport_candidate(s,db,"VHHH");
 verify(s.confirmed_airport->icao=="HECA"&&control(nd_controls(s,model),UiAction::focus).label=="VHHH","editing does not overwrite confirmed airport");
 s.query="NO MATCH";search_airports(s,db,0,0);
 verify(!confirm_airport_candidate(s)&&s.confirmed_airport->icao=="HECA"&&!control(nd_controls(s,model),UiAction::display_airport).enabled,"invalid draft cannot display stale selection");
 select_airport_candidate(s,db,"VHHH");verify(confirm_airport_candidate(s)&&s.confirmed_airport->icao=="VHHH"&&!s.airport_modifying,"valid confirmation atomically replaces details");
 scenario("ui/stale-feature-actions");s.panel=Panel::map_data;s.feature_kind=3;s.feature_selection=0;
 auto frame=nd_controls(s,model);verify(!control(frame,UiAction::add_cross).enabled&&!control(frame,UiAction::add_flag).enabled&&!control(frame,UiAction::feature_center).enabled,"missing OTHER feature cannot retain runway mark actions");
 return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
