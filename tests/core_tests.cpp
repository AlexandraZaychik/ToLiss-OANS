#include "core.hpp"
#include "route_plan.hpp"
#include "plan_airport.hpp"
#include "test_support.hpp"
#include <fstream>
#include <sstream>
#include <limits>
using namespace oans;
const char* fixture=
 "I\r\n1200 test\r\n1 0 0 0 XZ0001 Airport With Alias\r\n"
 "1302 icao_code ZTEST\r\n"
 "1302 iata_code tst\r\n"
 "1302 city Test City\r\n"
 "100 45 1 0 0.25 1 1 0 09 30.0 120.0 100 123 0 0 0 0 27 30.0 120.035 50 45 0 0 0 0\r\n"
 "110 1 0 0\r\n111 30.001 120.001\r\n112 30.001 120.005 30.002 120.004\r\n"
 "111 29.999 120.005\r\n113 29.999 120.001\r\n"
 "111 30.0002 120.002\r\n111 30.0002 120.003\r\n111 29.9998 120.003\r\n113 29.9998 120.002\r\n"
 "1300 30.002 120.004 90 gate jets Stand 42\r\n"
 "1301 C airline\r\n"
 "1300 30.002 120.005 90 gate jets A stand\r\n1301 A airline\r\n"
 "1300 30.002 120.005 90 gate jets B stand\r\n1301 B airline\r\n"
 "1300 30.002 120.005 90 gate jets D stand\r\n1301 D airline\r\n"
 "1300 30.002 120.005 90 gate jets E stand\r\n1301 E airline\r\n"
 "1300 30.002 120.005 90 gate jets F stand\r\n1301 F airline\r\n"
 "1300 30.002 120.010 90 gate jets Unknown\r\n"
 "1201 30.0 120.02 both 1\r\n1201 30.001 120.02 both 2\r\n"
 "1201 29.999 120.025 both 3\r\n1201 30.0 120.025 both 4\r\n"
 "1202 1 2 twoway taxiway A1\r\n1202 3 4 oneway taxiway ENTRY_ONLY\r\n"
 "1202 1 4 twoway runway 09/27\r\n99";

int main(){try{
    scenario("airport/parse-index-load");
    std::istringstream in(fixture);auto a=read_airport(in);
    verify(a.icao=="ZTEST"&&a.header_id=="XZ0001"&&a.iata=="TST"&&a.runways.size()==2,"ICAO authority and reciprocal runways");
    verify(a.runways[0].displaced==100&&a.runways[1].displaced==50&&a.runways[0].stopway_start==123&&a.runways[1].stopway_start==45,"directional threshold and stopway geometry");
    verify(a.pavements.size()==1&&a.pavements[0].rings.size()==2&&a.pavements[0].rings[0].size()>4&&a.stands.size()==4,"holes, curves and C-F stand filtering");
    const auto file=std::filesystem::temp_directory_path()/"oans-pruned-apt.dat";
    {std::ofstream out(file,std::ios::binary);out<<fixture;}
    auto index=scan_index(file);verify(index.size()==1&&load_airport(index[0]).icao==a.icao&&index[0].city=="Test City","index offset and metadata survive CRLF input");
    auto lower=index[0];lower.file="lower";lower.longest=9999;
    verify(canonical_index({index[0],lower}).size()==1&&find_airport({index[0],lower},"ZTEST")->file==file,"preferred scenery owns duplicate identity");
    std::filesystem::remove(file);

    scenario("airport/runway-pair-filter");
    IndexEntry e;e.icao="PAIR";e.longest=3000;e.runway_sizes={{3000,29},{1199,60}};
    verify(filter_airport_index({e},1200,30).empty(),"length and width must be satisfied by the SAME runway");
    e.runway_sizes.push_back({1200,30});verify(filter_airport_index({e},1200,30).size()==1,"exact dimensions qualify");

    scenario("airport/surface-tracker-crossing");
    Airport surface;surface.nodes={{1,{-100,0}},{2,{0,0}},{3,{100,0}},{4,{0,-100}},{5,{0,100}}};
    surface.edges={{1,2,false,false,"A"},{2,3,false,false,"A"},{4,2,false,false,"F"},{2,5,false,false,"F"}};
    SurfaceTracker tracker;
    verify(tracker.update(surface,{-10,0},90,0)=="A"&&tracker.update(surface,{0,0},0,.5)=="A"&&tracker.update(surface,{0,5},0,1)=="F","junction requires confirmed turn instead of nearest-edge flicker");
    verify(tracker.update(surface,{NAN,0},0,1.5).empty(),"invalid position clears stale surface");

    scenario("route/connected-departure-and-ribbon");
    Airport route;route.icao="ZTST";route.nodes={{1,{0,0}},{2,{100,0}},{3,{200,0}},{4,{300,0}}};
    route.edges={{1,2,false,false,"L1"},{2,3,false,false,"A"},{3,4,false,false,"A1"}};
    route.stands={{{-10,0},0,"Gate512Heavy"}};route.runways={{"02L","20R",{300,-500},{300,500},45}};
    auto r=make_route_plan(route,"[ZTST] 512 L1 A A1 RW02L");
    verify(bool(r)&&r.start_stand=="Gate512Heavy"&&r.points.front().x==-10&&r.points.back().x==300&&std::abs(r.metres-310)<1e-6,"tag, fuzzy stand and ordered connectivity");
    auto ribbon=route_ribbon(r.points);verify(!ribbon.empty()&&std::abs(ribbon[0][0].y-10)<1e-9,"route ribbon has 20 metre total width");

    scenario("route/airport-isolation");
    verify(!make_route_plan(route,"[ZUUU] 512 L1 A A1 RW02L"),"route cannot cross airport identity");
    scenario("route/disconnected-input");
    verify(!make_route_plan(route,"512 L1 A1 RW02L"),"named edges must connect in given order");
    scenario("route/projected-stand-oneway");
    Airport p;p.nodes={{1,{0,0}},{2,{100,0}},{3,{200,100}}};p.edges={{1,2,false,false,"C"},{2,3,false,true,"T1"}};p.stands={{{170,30},0,"25"}};
    r=make_route_plan(p,"C T1 25");verify(bool(r)&&length(r.points[r.points.size()-2]-Vec2{150,50})<1e-9,"stand joins interior edge at perpendicular projection");
    verify(!make_route_plan(p,"25 T1 C")&&p.nodes.size()==3&&p.edges.size()==2,"reverse partial one-way is rejected without mutating geometry");

    scenario("plan/crew-consultation-lifecycle");
    auto entry=[](const char* code,double lon){IndexEntry x;x.icao=code;x.lat=0;x.lon=lon;return x;};
    const std::vector<IndexEntry> db={entry("ORIG",0),entry("DEST",2),entry("OTHER",1)};
    FmsPlan fp;fp.observed=true;fp.origin.airport="ORIG";fp.destination.airport="DEST";PlanAirportSelection selection;
    verify(selection.select(db,fp,0,0,false)->icao=="ORIG"&&selection.select(db,fp,0,0,true)->icao=="DEST","short flight phase selects correct endpoint");
    selection.consult("DEST");verify(selection.select(db,fp,0,0,false)->icao=="DEST","crew destination preference survives automatic mode");
    selection.reset();fp.observed=false;verify(!selection.select(db,fp,0,0,true),"reset and unobserved plan cannot retain stale endpoints");
    scenario("plan/distance/300");verify(!plan_prefers_destination(false,true,300,100),"300 NM retains origin");
    scenario("plan/distance/350");verify(!plan_prefers_destination(false,true,350,100),"350 NM retains origin");
    scenario("plan/distance/50");verify(!plan_prefers_destination(false,false,351,50),"exactly 50 NM retains origin");
    scenario("plan/distance/above-50");verify(plan_prefers_destination(false,false,351,50.001),"beyond 50 NM selects destination");
    return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
