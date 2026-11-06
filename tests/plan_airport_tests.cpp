#include "plan_airport.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace oans;
namespace {
int checks=0;
void check(bool ok,const char* message) {++checks;if(!ok)throw std::runtime_error(message);}
IndexEntry airport(const std::string& code,double lat,double lon) {
    IndexEntry e;e.icao=code;e.lat=lat;e.lon=lon;return e;
}
}
int main() {
    try {
        const auto unknown=std::numeric_limits<double>::quiet_NaN();
        check(!plan_prefers_destination(false,false,200,100),"short flight outside enroute phase retains origin");
        check(plan_prefers_destination(false,true,299.999,0),"short flight in ECAM 6 selects destination");
        for(double distance:{300.,325.,350.})
            check(!plan_prefers_destination(false,true,distance,100),"300 through 350 NM retains origin");
        check(!plan_prefers_destination(false,false,350.001,50),"exactly 50 NM retains origin");
        check(!plan_prefers_destination(false,false,350.001,49.999),"inside 50 NM retains origin");
        check(plan_prefers_destination(false,false,350.001,50.001),"long flight beyond 50 NM selects destination");
        check(plan_prefers_destination(true,false,325,0),"last destination consultation overrides distance and phase");
        check(!plan_prefers_destination(false,true,unknown,100),"missing flight distance cannot select destination");
        check(!plan_prefers_destination(false,false,400,unknown),"missing position cannot satisfy 50 NM condition");
        check(!plan_prefers_destination(false,true,-1,100),"invalid flight distance is rejected");
        check(airport_distance_nm(0,0,0,0)==0,"coincident airport references");
        const double one_degree=earth*pi/180/1852;
        check(std::abs(airport_distance_nm(0,179,0,-179)-2*one_degree)<1e-8,"date-line distance follows shortest arc");
        check(std::abs(airport_distance_nm(0,0,0,180)-earth*pi/1852)<1e-8,"long-haul distance is spherical");
        check(airport_distance_nm(80,0,80,90)<1000,"polar distance does not use flat projection");
        for(const auto family:{"toliss","lr"}) {
            for(double phase:{0.,1.,2.,3.,4.,5.,7.,8.,9.,10.,6.5,unknown})
                check(!plan_enroute_phase(family,phase),"Airbus excludes all ECAM phases other than 6");
            check(plan_enroute_phase(family,6),"both Airbus families use ECAM 6 for climb/cruise/descent");
        }
        check(!plan_enroute_phase("zibo",6),"Zibo is outside the Airbus ECAM policy");
        check(!plan_enroute_phase("unknown",6),"unrecognized aircraft cannot supply an ECAM phase");

        const std::vector<IndexEntry> index={airport("ORIG",0,0),airport("DEST",0,2),airport("OTHER",0,1),airport("LONG",0,10)};
        FmsPlan plan;plan.observed=true;plan.origin.airport="ORIG";plan.destination.airport="DEST";
        PlanAirportSelection selection;
        auto opened=[&](double lat,double lon) {
            const auto result=selection.select_airborne_open(index,plan,lat,lon);return result?result->icao:"NONE";
        };
        check(opened(0,0)=="DEST","airborne open prefers destination over nearby airport");
        selection.consult("OTHER");
        check(opened(0,0)=="OTHER","crew selection overrides destination and nearest airport");
        selection.consult("MISSING");plan.destination.airport="MISSING";
        check(opened(0,0)=="ORIG","unavailable selected and destination airports fall back to nearest");
        check(opened(40,40)=="NONE","airborne open reports no airport outside 40 NM");
        plan.destination.airport="DEST";plan.observed=false;
        check(opened(0,0)=="ORIG","unobserved destination is not used");
        selection.reset();plan.observed=true;
        auto selected=[&](double lat,double lon,bool cruise) {
            const auto result=selection.select(index,plan,lat,lon,cruise);return result?result->icao:"NONE";
        };
        check(selected(0,0,false)=="ORIG","valid FMS origin is default without a prior consultation");
        check(selected(0,0,true)=="DEST","short-flight rule uses endpoint coordinates");
        check(selected(0,0,false)=="ORIG","automatic destination load does not become a crew consultation");
        selection.consult("DEST");
        check(selected(0,0,false)=="DEST","manual destination remains the default after returning to automatic mode");
        selection.consult("OTHER");
        check(selected(0,0,false)=="ORIG","consulting another airport replaces destination history");
        plan.destination.airport="LONG";
        check(selected(0,0,false)=="ORIG"&&selected(0,1,false)=="LONG","long-flight rule uses distance from origin");
        selection.consult("DEST");
        check(selected(0,0,false)=="ORIG","changing FMS destination invalidates old destination preference");
        selection.consult("LONG");selection.reset();
        check(selection.last_consulted().empty()&&selected(0,0,false)=="ORIG","new flight clears consultation history");
        plan.destination.airport="MISSING";
        check(selected(0,0,true)=="ORIG","unavailable destination falls back to valid origin");
        plan.origin.airport="MISSING";
        check(selected(0,0,true)=="NONE","missing endpoints allow host nearest fallback");
        plan.destination.airport="DEST";selection.consult("DEST");
        check(selected(0,0,false)=="DEST","last destination consultation works with partial FMS origin");
        plan.observed=false;
        check(selected(0,0,true)=="NONE","unobserved flight plan cannot select stale endpoints");
        std::cout<<"PASS "<<checks<<" PLAN airport checks\n";
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
