#include "core.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace oans;
int checks=0;
void check(bool value,const char* message) {++checks;if(!value)throw std::runtime_error(message);}
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
void test_airport() {
    std::istringstream in(fixture);auto a=read_airport(in);
    check(a.icao=="ZTEST"&&a.header_id=="XZ0001"&&a.icao_explicit,"1302 ICAO overrides XZ0 header ident");
    check(a.iata=="TST","IATA metadata normalized separately from ICAO");
    check(a.runways.size()==2,"reciprocal runways");
    check(a.runways[0].name=="09"&&a.runways[1].name=="27","runway designators");
    check(a.runways[0].displaced==100&&a.runways[1].displaced==50,"displaced thresholds per direction");
    check(a.runways[0].stopway_start==123&&a.runways[0].stopway_end==45,"physical end overrun lengths parsed");
    check(a.runways[1].stopway_start==45&&a.runways[1].stopway_end==123,"overrun ends reverse with direction");
    check(a.runways[0].length()>3300&&a.runways[0].length()<3400,"geographic projection in meters");
    check(a.pavements.size()==1&&a.pavements[0].rings.size()==2,"pavement holes stay separate contours");
    check(a.pavements[0].rings[0].size()>4,"Bezier curve flattened");
    check(a.stands.size()==4&&a.stands[0].name=="Stand 42","stand name keeps spaces");
    check(a.stands[0].category=='C'&&a.stands[1].category=='D'&&a.stands[2].category=='E'&&a.stands[3].category=='F',"only explicitly classified C-F stands survive");
    check(stand_name("new ramp 32L")=="32L"&&stand_name("A12B")=="A12B"&&stand_name("32L")=="32L","simple names preserve short names and shorten long names from first digit");
    check(stand_name("M21")=="M21"&&stand_name("ABC1234")=="ABC1234","simple names below eight characters keep letter prefixes");
    check(stand_name("ABCD1234")=="ABCD1234","eight-character names stay complete");
    check(stand_name(u8"机位M1234")==u8"机位M1234"&&stand_name(u8"机位MN1234")==u8"机位MN1234","name limit counts UTF-8 characters rather than bytes");
    check(stand_name("new ramp start 1")=="1"&&stand_name("new ramp start M7")=="M7"&&stand_name("start M7")=="start M7"&&stand_name("new ramp startM7289")=="7289","long names keep last space block before digit fallback");
    check(stand_name("Aircraft North")=="North"&&stand_name("Park Stand_12")=="Stand_12"&&stand_name("new ramp startM7289",false)=="new ramp startM7289","last block and complex names");
    check(stand_name("Cargo")=="Cargo"&&stand_name("").empty(),"digit-free stand names remain unchanged");
    check(stand_name("new ramp 32L",false)=="new ramp 32L","complex names retain original text");
    auto file=std::filesystem::temp_directory_path()/"toliss_oans_test_apt.dat";
    {std::ofstream out(file,std::ios::binary);out<<fixture;}
    auto idx=scan_index(file);check(idx.size()==1&&idx[0].icao=="ZTEST","CRLF and final line without newline index");
    check(idx[0].runway_sizes.size()==1&&idx[0].runway_sizes[0].width==45&&std::abs(idx[0].runway_sizes[0].length-idx[0].longest)<1e-9,"index retains paired physical runway length and width");
    check(idx[0].iata=="TST"&&load_airport(idx[0]).iata=="TST","IATA survives index and airport loading");
    check(idx[0].city=="Test City","city metadata keeps full multiword value for CITY NAME selector");
    check(std::abs(idx[0].lon-120.0175)<1e-9,"missing ARP falls back to runway midpoint");
    check(std::abs(a.reference.lon-idx[0].lon)<1e-9,"index and loaded fallback reference agree");
    for(const auto& metadata:std::vector<std::string>{
        "1302 datum_lat 30.004\n1302 datum_lon 120.008\n",
        "1302 datum_lon 120.008\n1302 datum_lat 30.004\n",
        "1302 datum_lat 30.004\n",
        "1302 datum_lat nan\n1302 datum_lon 120.008\n",
        "1302 datum_lat 91\n1302 datum_lon 120.008\n"}) {
        std::string text(fixture);text.insert(text.rfind("99"),metadata);
        {std::ofstream out(file,std::ios::binary);out<<text;}
        auto entry=scan_index(file).at(0);auto loaded=load_airport(entry);
        const bool valid=metadata.find("30.004")!=std::string::npos&&metadata.find("datum_lon")!=std::string::npos;
        check(std::abs(entry.lat-(valid?30.004:30))<1e-9&&std::abs(entry.lon-(valid?120.008:120.0175))<1e-9,"ARP pair validated independently of metadata order; incomplete or invalid pairs use fallback");
        check(std::abs(loaded.reference.lat-entry.lat)<1e-9&&std::abs(loaded.reference.lon-entry.lon)<1e-9,"index and loaded ARP agree");
        check(loaded.lat==30&&loaded.lon==120&&length(loaded.runways[0].start)<1e-9,"ARP does not shift geometry projection origin");
    }
    {std::ofstream out(file,std::ios::binary);out<<fixture;}
    auto no_city=idx[0];no_city.city.clear();
    check(canonical_index({no_city,idx[0]})[0].city=="Test City","missing city is supplemented from matching explicit ICAO");
    auto no_iata=idx[0];no_iata.iata.clear();
    auto combined=canonical_index({no_iata,idx[0]});
    check(combined.size()==1&&combined[0].iata=="TST"&&combined[0].file==file,"missing scenery IATA filled from matching ICAO without geometry change");
    auto conflicting=idx[0];conflicting.iata="ABC";
    check(canonical_index({conflicting,idx[0]})[0].iata=="ABC","preferred scenery IATA retains precedence");
    check(load_airport(idx[0]).icao=="ZTEST","seek offset loads exact airport header");
    auto duplicate=idx[0];duplicate.file="low-priority";duplicate.lat=0;
    check(nearest({idx[0],duplicate},30,120)->file==file,"scenery priority wins aliases");
    check(!nearest(idx,0,0),"nearest has radius boundary");std::filesystem::remove(file);
    auto anti=local(0,-179.999,0,179.999);check(std::abs(anti.x)<300,"antimeridian projection");
}
void test_markings() {
    std::string input(fixture);input.resize(input.rfind("99"));
    input+="120 Center and curve\n111 30 120 1 101\n112 30.001 120.001 30.001 120 2\n115 30.002 120.002\n"
           "111 30.003 120.003 101\n115 30.004 120.004\n"
           "120 Closed\n111 30 120 3\n111 30 120.001 4\n113 30.001 120.001 5\n"
           "120 Black-bordered\n111 30 120 51\n116 30.001 120.001 30.002 120.001\n"
           "120 Extended centerline\n111 30.002 120.010 60\n115 30.003 120.010\n"
           "120 Wide ILS critical centerline\n111 30.002 120.020 11\n115 30.003 120.020\n"
           "120 Black wide ILS critical centerline\n111 30.002 120.030 61\n115 30.003 120.030\n"
           "120 Wide ILS hold\n111 30.002 120.040 14\n115 30.003 120.040\n"
           "120 Black wide ILS hold\n111 30.002 120.050 64\n115 30.003 120.050\n"
           "130 Airport boundary\n111 30 120 1\n113 30.001 120.001 1\n99";
    std::istringstream in(input);auto a=read_airport(in);
    check(a.markings.size()==11,"only actual paint segments, no light strings, boundaries or phantom joins");
    check(a.markings[0].style==1&&a.markings[1].style==2,"style starts at its node, not the destination");
    check(a.markings[0].points.size()==13&&a.markings[1].points.size()==13,"incoming and outgoing Bezier controls are flattened");
    check(length(a.markings[0].points.back()-a.markings[1].points.front())<1e-8,"connected markings share the exact endpoint");
    check(a.markings[2].style==3&&a.markings[3].style==4&&a.markings[4].style==5,"closed chain preserves per-segment style");
    check(length(a.markings[4].points.back()-a.markings[2].points.front())<1e-8,"113 closes to the first node");
    check(a.markings[5].style==51&&a.markings[5].points.size()==13,"116 ends a curved open chain without closure");
    check(a.markings[6].style==60,"retain the centerline style recognized by the A350 map parser");
    check(a.markings[7].style==11&&a.markings[8].style==61,"retain wide ILS critical centerline styles");
    check(a.markings[9].style==14&&a.markings[10].style==64,"retain wide ILS hold styles");
    check(a.pavements.size()==1&&a.pavements[0].rings.size()==2,"linear features do not become pavement polygons");
}
void test_identifier_priority() {
    auto file=std::filesystem::temp_directory_path()/"toliss_oans_id_priority_apt.dat";
    auto runway=[](double lat) {std::ostringstream s;s<<"100 45 1 0 0.25 1 1 0 09 "<<lat<<" 120.0 0 0 0 0 0 0 27 "<<lat<<" 120.035 0 0 0 0 0 0\n";return s.str();};
    {
        std::ofstream out(file,std::ios::binary);
        out<<"I\n1200 test\n1 0 0 0 ZSQD Old header fallback\n"<<runway(30)
           <<"1 0 0 0 XZ0NEW Explicit airport\n"<<runway(31)<<"  1302\ticao_code\tzsqd\n"
           <<"1 0 0 0 OTHER_PREFIX Duplicate explicit lower priority\n1302 icao_code ZSQD\n"<<runway(32)
           <<"1 0 0 0 ANY_PREFIX Different alias\n1302 icao_code ZXYZ\n"<<runway(33)
           <<"1 0 0 0 XZ0NO No ICAO metadata\n"<<runway(34)<<"99";
    }
    auto idx=scan_index(file);
    check(idx.size()==5,"all airport headers indexed");
    check(!idx[0].icao_explicit&&idx[0].icao=="ZSQD","header-only record marked as fallback");
    check(idx[1].icao_explicit&&idx[1].icao=="ZSQD"&&idx[1].header_id=="XZ0NEW","late tab-separated 1302 overrides XZ0 ident");
    auto selected=find_airport(idx,"zsqd");
    check(selected&&selected->header_id=="XZ0NEW","explicit metadata beats an earlier header fallback");
    check(load_airport(*selected).header_id=="XZ0NEW"&&load_airport(*selected).icao_explicit,"selected byte offset loads explicit airport");
    auto canonical=canonical_index(idx);
    check(canonical.size()==3&&canonical[0].header_id=="XZ0NEW","canonical index resolves duplicate identity once");
    check(find_airport(canonical,"ZSQD")->header_id==selected->header_id,"canonical and raw lookup agree");
    check(find_airport(idx,"ZXYZ")->header_id=="ANY_PREFIX","1302 authority does not depend on header prefix");
    check(!find_airport(idx,"ANY_PREFIX"),"explicitly remapped ident is not a competing ICAO key");
    auto internal=find_airport(idx,"XZ0NO");
    check(internal&&!internal->icao_explicit&&load_airport(*internal).header_id=="XZ0NO","airport without metadata remains a local identifier");
    check(nearest(idx,31,120)->header_id=="XZ0NEW","nearest and manual lookup use same explicit identity");
    check(!nearest(idx,30,120),"nearby stale header record cannot stand in for a different explicit airport");
    auto explicit_high=idx[1],explicit_low=idx[2];
    check(find_airport({explicit_high,explicit_low},"ZSQD")->header_id==explicit_high.header_id,"explicit ties keep scenery order");
    check(find_airport({idx[0]},"ZSQD")->header_id=="ZSQD","metadata absence retains header lookup fallback");
    std::filesystem::remove(file);
}
void test_scenery_priority() {
    const auto root=std::filesystem::temp_directory_path()/
        ("toliss_oans_scenery_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(std::filesystem::create_directory(root),"isolated scenery fixture directory created");
    const auto custom=root/"Custom Scenery";
    auto apt=[&](const std::filesystem::path& pack) {
        auto p=root/pack/"Earth nav data/apt.dat";
        std::filesystem::create_directories(p.parent_path());
        std::ofstream out(p,std::ios::binary);out<<fixture;return p;
    };
    const auto global=apt("Global Scenery/Global Airports");
    const auto legacy=apt("Custom Scenery/Global Airports");
    const auto defaults=apt("Resources/default scenery/default apt dat");
    const auto a=apt("Custom Scenery/A Airport");
    const auto b=apt("Custom Scenery/B Airport");
    const auto unlisted=apt(std::filesystem::u8path(u8"Custom Scenery/C 未登记机场"));
    const auto disabled=apt("Custom Scenery/Disabled Airport");
    auto write_ini=[&](const std::string& value) {std::ofstream out(custom/"scenery_packs.ini",std::ios::binary);out<<value;};
    const auto config="SCENERY_PACK *GLOBAL_AIRPORTS*\r\n"
        "SCENERY_PACK Custom Scenery/Global Airports/\r\n"
        "SCENERY_PACK Custom Scenery/B Airport/\r\n"
        "SCENERY_PACK Custom Scenery/A Airport/\r\n"
        "SCENERY_PACK Custom Scenery/B Airport/\r\n"
        "SCENERY_PACK Custom Scenery/Missing/\r\n"
        "SCENERY_PACK_DISABLED Custom Scenery/Disabled Airport/\r\n";
    write_ini(config);
    check(scenery_files(root)==std::vector<std::filesystem::path>{b,a,unlisted,global,legacy,defaults},
          "custom order, unlisted Unicode pack, disabled exclusion, deduplication and default fallback");
    std::vector<IndexEntry> entries;
    for(const auto& p:scenery_files(root)) {
        auto scanned=scan_index(p);entries.insert(entries.end(),scanned.begin(),scanned.end());
    }
    check(find_airport(entries,"ZTEST")->file==b,"manual lookup prefers custom airport below global marker");
    auto canonical=canonical_index(entries);
    check(canonical.size()==1&&canonical[0].file==b,"plugin canonical index uses custom scenery geometry");
    check(nearest(entries,30,120)->file==b,"automatic airport selection agrees with custom scenery priority");
    check(load_airport(canonical[0]).name=="Airport With Alias","selected custom airport offset loads successfully");
    auto header_only=entries[0];header_only.icao_explicit=false;header_only.header_id="ZTEST";
    auto lower_explicit=entries.back();
    check(find_airport({header_only,lower_explicit},"ZTEST")->file==b,
          "lower-priority explicit ICAO cannot override a custom header-only airport");
    check(canonical_index({header_only,lower_explicit})[0].file==b,
          "canonical index retains custom file priority without explicit ICAO metadata");
    check(nearest({header_only,lower_explicit},30,120)->file==b,
          "nearest retains custom file priority without explicit ICAO metadata");
    write_ini(std::string(config)+"SCENERY_PACK_DISABLED Custom Scenery/Global Airports/\n");
    check(scenery_files(root)==std::vector<std::filesystem::path>{b,a,unlisted,global,defaults},
          "legacy global airport respects disabled entry");
    std::filesystem::remove(custom/"scenery_packs.ini");
    check(scenery_files(root)==std::vector<std::filesystem::path>{a,b,unlisted,disabled,global,legacy,defaults},
          "missing ini still discovers custom airports in deterministic order");
    std::filesystem::remove_all(root);
    check(scenery_files(root).empty(),"missing scenery root returns no sources");
}
void test_projection() {
    std::array<Vec2,4> q={Vec2{-300,700},Vec2{500,650},Vec2{440,100},Vec2{-250,130}};
    auto h=Homography::quad(q);check(h.has_value(),"skewed quad transform");auto inv=h->inverse();
    for(auto v:{Vec2{0,0},Vec2{1,0},Vec2{1,1},Vec2{0,1},Vec2{.5,.5},Vec2{.125,.875}})
        check(length(inv->map(h->map(v))-v)<1e-10,"perspective click round trip including negative desktop origin");
    std::array<Vec2,4> flat{};check(!Homography::quad(flat),"degenerate screen rejected");
}
void test_aircraft_airport_filter() {
    for(const auto& code:{"A319","a19n","A320","a20n","A321","a21n"})
        check(minimum_runway_length(code)==1200,"A320 family minimum is 1200 m, case insensitive");
    check(minimum_runway_length("a339")==1600&&minimum_runway_length("a346")==1700,"widebody runway limits");
    check(minimum_runway_length("")==0&&minimum_runway_length("B738")==0,"unspecified aircraft retain previous airport coverage");
    std::vector<IndexEntry> all;
    for(double length:{1199.0,1200.0,1599.0,1600.0,1699.0,1700.0,3000.0}) {
        IndexEntry e;e.icao="T"+std::to_string(all.size());e.longest=length;e.runway_sizes={{length,45}};e.lat=30;e.lon=120;all.push_back(e);
    }
    for(const auto& code:{"A320","A339","A346"}) {
        const auto minimum=minimum_runway_length(code);
        const auto filtered=filter_airport_index(all,minimum);
        check(filtered.front().longest==minimum,"exact runway limit remains eligible");
        for(const auto& e:all)check(bool(find_airport(filtered,e.icao))==(e.longest>=minimum),"lookup uses longest runway threshold");
        check(nearest(filtered,30,120)->longest==minimum,"nearest selection excludes short-runway airports");
    }
    check(filter_airport_index(all,1200).size()==6&&filter_airport_index(all,1700).size()==2&&filter_airport_index(all,1200).size()==6,"aircraft changes refilter the complete source index");
    auto preferred=all[0],fallback=preferred;preferred.file="custom";fallback.file="global";fallback.longest=3000;fallback.runway_sizes={{3000,45}};
    check(filter_airport_index(canonical_index({preferred,fallback}),1200).empty(),"short preferred scenery cannot reappear through a longer fallback");
    check(filter_airport_index(all,0).size()==all.size()&&filter_airport_index({},1700).empty(),"unknown aircraft and empty database handling");
    for(const auto& code:{"A319","a19n","A320","a20n","A321","a21n","a339","A346"}) {
        const auto length=minimum_runway_length(code),width=minimum_runway_width(code);
        check(width==(length==1200?30:45),"aircraft runway width thresholds are case insensitive");
        IndexEntry e;e.icao="PAIR";e.longest=3000;
        e.runway_sizes={{3000,width-1},{length-1,60}};
        check(filter_airport_index({e},length,width).empty(),"long narrow and short wide runways cannot satisfy requirements across runways");
        e.runway_sizes.push_back({length,width});
        check(filter_airport_index({e},length,width).size()==1,"a shorter runway meeting both exact limits qualifies the airport");
        e.runway_sizes={{length-1,width},{length,width-1}};
        check(filter_airport_index({e},length,width).empty(),"either dimension below its threshold rejects the runway");
    }
    check(minimum_runway_width("B738")==0&&minimum_runway_width("")==0,"unlisted aircraft have no added width restriction");
    const auto file=std::filesystem::temp_directory_path()/"toliss_oans_runway_width_test.dat";
    std::string data(fixture);data.replace(data.find("100 45"),6,"100 20");
    data.insert(data.rfind("99"),"100 45 1 0 0.25 1 1 0 18 30.0 120.0 0 0 0 0 0 0 36 30.015 120.0 0 0 0 0 0 0\n");
    {std::ofstream out(file,std::ios::binary);out<<data;}
    const auto parsed=scan_index(file);std::filesystem::remove(file);
    check(parsed[0].runway_sizes.size()==2&&parsed[0].runway_sizes[0].width==20&&parsed[0].runway_sizes[1].width==45,"scanner retains non-longest runway dimensions");
    check(filter_airport_index(parsed,1600,45).size()==1&&filter_airport_index(parsed,1700,45).empty(),"parsed shorter wide runway qualifies A339 but not A346 despite a longer narrow runway");
}
void test_airport_search_radius() {
    IndexEntry e;e.icao="RANGE";e.lon=0;
    const auto at_nm=[&](double nm) {e.lat=nm*1852.0/earth*180/pi;return nearest({e},0,0).has_value();};
    check(at_nm(25),"airport beyond 40 km remains within the 40 NM search radius");
    check(at_nm(39.99),"airport just inside 40 NM is available");
    check(!at_nm(40.01),"airport beyond 40 NM is unavailable");
}
void test_surface_name() {
    Airport a;
    a.runways.push_back({"09","27",{0,0},{1000,0},40});
    a.stands.push_back({{300,100},90,"new ramp 27",'C'});
    a.nodes={{1,{0,100}},{2,{1000,100}},{3,{0,150}},{4,{1000,150}}};
    a.edges={{1,2,false,false,"A"},{3,4,false,false,"B"},{1,99,false,false,"BAD"}};
    check(surface_name(a,{500,0},90)=="09","runway end follows aircraft heading");
    check(surface_name(a,{500,0},270)=="27","reciprocal runway end");
    check(surface_name(a,{500,20},90)=="09","physical runway edge included");
    check(surface_name(a,{500,20.01},90).empty(),"no runway shoulder attraction");
    check(surface_name(a,{-1,0},90).empty(),"no runway extension attraction");
    check(surface_name(a,{300,100},90)=="27","stand wins over taxi route and uses simple name");
    check(surface_name(a,{300,100},90,false).empty(),"complete stand name beyond five characters is suppressed");
    check(surface_name(a,{300,120},90)=="27","stand 20 m boundary");
    check(surface_name(a,{300,120.01},90).empty(),"no remote stand attraction");
    check(surface_name(a,{500,114.99},90)=="A","named taxi route corridor");
    check(surface_name(a,{500,115.01},90).empty(),"outside taxi corridor");
    check(surface_name(a,{500,149},90)=="B","nearest parallel taxi route");
    check(surface_name(a,{-1,100},90).empty(),"taxi route cannot extend past endpoint");
    a.edges[0].runway=true;
    check(surface_name(a,{500,100},90).empty(),"runway routing edge cannot impersonate taxiway");
    a.edges[0].runway=false;a.edges[0].name.clear();
    check(surface_name(a,{500,100},90).empty(),"unnamed route is blank");
    a.nodes[3]={0,110};a.nodes[4]={1000,110};
    check(surface_name(a,{500,100},90).empty(),"anonymous nearest route does not borrow parallel named route");
    check(surface_name(a,{500,110},90)=="B","named neighbour remains identifiable at its own centreline");
    a.nodes[3]={0,100};a.nodes[4]={1000,100};
    check(surface_name(a,{500,100},90).empty(),"anonymous route wins a distance tie against named route");
    std::reverse(a.edges.begin(),a.edges.end());
    check(surface_name(a,{500,100},90).empty(),"anonymous tie behavior is independent of edge order");
    a.edges.clear();a.edges.push_back({1,2,false,false,"ABCDE"});
    check(surface_name(a,{500,100},90)=="ABCDE","five characters displayed completely");
    a.edges[0].name="ABCDEF";
    check(surface_name(a,{500,100},90).empty(),"six characters suppress the whole label");
    a.edges[0].name=u8"一二三四五";
    check(surface_name(a,{500,100},90)==u8"一二三四五","character limit counts UTF-8 characters rather than bytes");
    a.edges[0].name=u8"一二三四五六";
    check(surface_name(a,{500,100},90).empty(),"six UTF-8 characters are suppressed");
    std::string anonymous_data(fixture);
    const auto route=anonymous_data.find("taxiway A1");anonymous_data.replace(route,10,"taxiway");
    std::istringstream anonymous_stream(anonymous_data);const auto parsed=read_airport(anonymous_stream);
    check(parsed.edges.size()==3&&parsed.edges[0].name.empty(),"apt.dat anonymous taxi edge survives parsing");
    const auto& anonymous=parsed.edges[0];
    check(surface_name(parsed,(parsed.nodes.at(anonymous.a)+parsed.nodes.at(anonymous.b))*.5,0).empty(),"parsed anonymous taxiway produces no label");
    check(surface_name(a,{NAN,0},90).empty()&&surface_name(a,{0,0},NAN).empty(),"invalid position or heading is blank");
    a.stands.push_back({{500,0},90,"CONFLICT",'C'});
    check(surface_name(a,{500,0},90)=="09","physical runway wins over nearby stand");
}
void test_surface_tracker() {
    // Rotated, split A/F intersection matching the supplied ZGDY topology.
    const double angle=20*pi/180;
    const Vec2 along{std::cos(angle),std::sin(angle)},across{-along.y,along.x};
    const auto point=[&](double x,double y) {return along*x+across*y;};
    Airport a;a.nodes={{1,point(-100,0)},{2,point(0,0)},{3,point(100,0)},
                       {4,point(0,-100)},{5,point(0,100)}};
    a.edges={{1,2,false,false,"A"},{2,3,false,false,"A"},{4,2,false,false,"F"},{2,5,false,false,"F"}};
    for(int order=0;order<2;++order) {
        SurfaceTracker tracker;double now=0;
        for(double x:{-20.0,-10.0,-3.0,-.5,0.0,.5,3.0,10.0,20.0}) {
            const double offset=(int(now*2)%2)?1.2:-1.2;
            check(tracker.update(a,point(x,offset),70,now)=="A","straight crossing retains A despite nearest-edge changes");now+=.5;
        }
        tracker.reset();
        check(tracker.update(a,point(-10,0),70,0)=="A","initial A route acquired");
        check(tracker.update(a,point(0,0),-20,.5)=="A","one crossing sample cannot switch to F");
        check(tracker.update(a,point(0,0),70,1)=="A","transient heading error cancels route switch");
        check(tracker.update(a,point(0,2),-20,1.5)=="A","real turn starts confirmation");
        check(tracker.update(a,point(0,5),-20,2)=="F","confirmed turn switches once to F");
        check(tracker.update(a,point(.5,10),-20,2.5)=="F","F persists after crossing");
        check(tracker.update(a,point(80,80),-20,3).empty(),"leaving all route corridors clears immediately");
        std::reverse(a.edges.begin(),a.edges.end());
    }
    for(auto& e:a.edges)if(e.name=="F")e.name.clear();
    SurfaceTracker tracker;
    check(tracker.update(a,point(-10,0),70,0)=="A","named route before anonymous crossing");
    check(tracker.update(a,point(0,0),70,.5)=="A","anonymous crossing cannot blank a straight named route");
    check(tracker.update(a,point(0,2),-20,1)=="A","anonymous turn also requires confirmation");
    check(tracker.update(a,point(0,5),-20,1.5).empty(),"confirmed anonymous route remains blank");
    check(tracker.update(a,point(0,0),-20,2).empty(),"anonymous route identity stays stable at crossing");
    check(tracker.update(a,point(0,0),70,2.5).empty(),"leaving anonymous route starts named confirmation");
    check(tracker.update(a,point(5,0),70,3)=="A","confirmed return to named A");
    Airport other=a;
    for(auto& e:other.edges)if(e.name=="A")e.name="B";
    check(tracker.update(other,point(5,0),70,3.5)=="B","airport replacement discards prior taxi identity");
    check(tracker.update(a,point(5,0),70,0)=="A","clock reset discards prior confirmation");
    check(tracker.update(a,{NAN,0},70,.5).empty(),"invalid position clears tracker");
    tracker.reset();check(tracker.update(a,point(0,5),-20,1).empty(),"reset does not retain an old named route");
}
void test_airport_crossing(const Airport& a) {
    for(const auto& node:a.nodes) {
        std::vector<Vec2> route,crossing;
        for(const auto& e:a.edges)if(!e.runway&&(e.a==node.first||e.b==node.first)) {
            const auto end=a.nodes.find(e.a==node.first?e.b:e.a);if(end==a.nodes.end())continue;
            if(e.name=="A")route.push_back(end->second);
            if(e.name=="F")crossing.push_back(end->second);
        }
        if(route.size()!=2||crossing.empty())continue;
        const auto center=node.second;
        const auto bearing=[](Vec2 d) {return std::atan2(d.x,d.y)*180/pi;};
        for(int direction=0;direction<2;++direction) {
            SurfaceTracker tracker;double now=0;
            const auto from=route[direction],to=route[1-direction];
            const auto before=(center-from)*(1/length(center-from)),after=(to-center)*(1/length(to-center));
            std::cout<<a.icao<<" A/F node="<<node.first<<" direction="<<direction<<" straight:";
            for(double d:{-25.0,-15.0,-5.0,-1.0,0.0,1.0,5.0,15.0,25.0}) {
                const auto axis=d<0?before:after;
                const Vec2 normal{-axis.y,axis.x};
                const auto found=tracker.update(a,center+axis*d+normal*((int(now*2)%2)?1.2:-1.2),bearing(axis),now);
                check(found=="A","actual airport A/F crossing retains A in both travel directions");
                std::cout<<' '<<found;now+=.5;
            }
            std::cout<<'\n';
        }
        SurfaceTracker tracker;const auto f=(crossing.front()-center)*(1/length(crossing.front()-center));
        const auto approaching=(center-route[0])*(1/length(center-route[0]));
        check(tracker.update(a,center-approaching*20,bearing(approaching),0)=="A","actual airport approach acquires A");
        check(tracker.update(a,center,bearing(f),.5)=="A","actual airport turn waits for confirmation");
        check(tracker.update(a,center+f*5,bearing(f),1)=="F","actual airport confirmed turn acquires F");
        check(tracker.update(a,center+f*15,bearing(f),1.5)=="F","actual airport turn stays on F");
        std::cout<<a.icao<<" A/F node="<<node.first<<" turn: A A F F\n";
        return;
    }
    throw std::runtime_error("Requested airport has no shared A/F crossing with two A branches");
}
int main(int argc,char** argv) {
    try {test_surface_tracker();test_surface_name();test_airport_search_radius();test_airport();test_aircraft_airport_filter();test_markings();test_identifier_priority();test_scenery_priority();test_projection();
        if(argc==3) {
            const auto entry=find_airport(scan_index(std::filesystem::u8path(argv[1])),argv[2]);
            check(entry.has_value(),"actual airport fixture exists");test_airport_crossing(load_airport(*entry));
        }
        std::cout<<"PASS "<<checks<<" checks\n";return 0;}
    catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<" after "<<checks<<" checks\n";return 1;}
}
