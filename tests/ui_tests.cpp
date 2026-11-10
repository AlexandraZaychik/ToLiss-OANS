#include "nd_ui.hpp"
#include "map_viewport.hpp"
#include "test_support.hpp"
using namespace oans;
const UiControl& control(const UiFrame& f,UiAction action){for(const auto& c:f.controls)if(c.action==action)return c;throw std::runtime_error("missing UI control");}
int main(){try{
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
