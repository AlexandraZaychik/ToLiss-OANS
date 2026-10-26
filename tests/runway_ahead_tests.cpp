#include "runway_ahead.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace oans;
int checks=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
Airport fixture() {
    Airport a;a.icao="TEST";
    a.runways={{"09","27",{0,0},{1000,0},40,100,0,0},
               {"27","09",{1000,0},{0,0},40,100,0,0}};
    return a;
}
RunwayAheadInput taxi(){RunwayAheadInput t;t.ground=true;t.speed=10;t.heading=0;t.position={500,-1000};return t;}
void warm(RunwayAhead& c,Airport& a,RunwayAheadInput& t) {
    t=taxi();for(int i=0;i<11;++i)c.update(&a,t,1);
}
int main(){try {
    Airport crossing;crossing.icao="CROSS";
    crossing.runways={{"05","23",{-1500,-1000},{1500,1000},45},
                      {"14","32",{-1000,1000},{1000,-1000},45}};
    const std::vector<RunwayAheadHit> crossing_hits={{"05","23",3,120},{"14","32",2,60}};
    const auto south=orient_runway_advisories(&crossing,crossing_hits,180);
    check(south[1].first=="32"&&south[1].second=="14","taxiway A southbound approach puts physical 32 threshold on left");
    check(south[0].first=="23"&&south[0].second=="05","each crossing runway uses its own projected threshold direction");
    check(south[0].seconds==3&&south[0].distance==120&&south[1].seconds==2&&south[1].distance==60,"orientation preserves row order and threat metrics");
    check(crossing_hits[1].first=="14","display orientation does not mutate detector state");
    const auto north=orient_runway_advisories(&crossing,south,0);
    check(north[1].first=="14"&&north[1].second=="32","opposite approach swaps the threshold labels back");
    check(orient_runway_advisories(&crossing,crossing_hits,-180)[1].first=="32"&&
          orient_runway_advisories(&crossing,crossing_hits,540)[1].first=="32","heading wraps consistently");
    check(orient_runway_advisories(nullptr,crossing_hits,180)[1].first=="14","missing airport preserves input");
    check(orient_runway_advisories(&crossing,crossing_hits,std::numeric_limits<double>::quiet_NaN())[1].first=="14","invalid heading preserves input");
    check(orient_runway_advisories(&crossing,{{"UNKNOWN","NONE",0,0}},180)[0].first=="UNKNOWN","unmatched runway pair preserves input");
    auto inline_airport=fixture();
    check(orient_runway_advisories(&inline_airport,{{"09","27",0,0}},90)[0].first=="09","fore/aft alignment has no invented left/right side");
    std::reverse(inline_airport.runways.begin(),inline_airport.runways.end());
    check(orient_runway_advisories(&inline_airport,{{"09","27",0,0}},180)[0].first=="27","reciprocal geometry record yields the same physical ordering");
    auto a=fixture();RunwayAhead c;auto t=taxi();
    check(test_runways(a,"9")[0].first=="09","test input normalizes single digit runway");
    check(test_runways(a,"RWY09/27").size()==1,"test input resolves and deduplicates reciprocal ends");
    auto bad_input=[&](const std::string& s) {try {test_runways(a,s);return false;}catch(const std::invalid_argument&){return true;}};
    check(bad_input(""),"test rejects empty runway");check(bad_input("99"),"test rejects invalid runway number");
    check(bad_input("08"),"test rejects runway absent at selected airport");
    a.runways.push_back({"10L","28R",{0,40},{1000,40},40});
    check(test_runways(a,"09,10l").size()==2,"test supports two actual runway pairs");
    check(test_runways(a,"28r")[0].first=="10L","test reciprocal endpoint resolves actual pair");
    a.runways.push_back({"11","29",{0,80},{1000,80},40});
    check(bad_input("09,10L,11"),"test rejects more than two physical runways");a=fixture();
    check(std::abs(RunwayAhead::nose+RunwayAhead::tail-37.57)<1e-9,"manual fuselage length");
    check(RunwayAhead::half_span==17.9,"manual half wingspan");
    t.position={500,-80-RunwayAhead::nose-70};
    for(int i=0;i<10;++i)check(c.update(&a,t,1).empty(),"strictly more than ten seconds below forty knots");
    auto hits=c.update(&a,t,.01);check(hits.size()==1,"trigger at seven seconds; reciprocal deduplicated");
    check(std::abs(hits[0].seconds-7)<1e-6,"nose offset used in time to entry");
    t.position.y-=.1;check(c.update(&a,t,.01).empty(),"lost prediction clears immediately without minimum display duration");
    t.position.y+=.1;check(c.update(&a,t,.1).size()==1,"rearm after prediction clears");
    t.paused=true;auto low=c.low_speed_seconds();c.update(&a,t,100);
    check(c.low_speed_seconds()==low&&c.hits().size()==1,"pause freezes timers and annunciation");t.paused=false;
    t.heading=180;check(c.update(&a,t,.01).empty(),"turn away clears warning immediately");
    t.heading=0;check(c.update(&a,t,.1).size()==1,"turn back predicts again");
    t.speed=0;check(c.update(&a,t,.01).empty(),"stop immediately clears warning without minimum display duration");
    check(c.update(&a,t,10).empty(),"stopped aircraft does not retrigger warning");
    t.speed=40*kt;check(c.update(&a,t,1).empty()&&c.low_speed_seconds()==0,"forty knots resets qualification");
    t.speed=10;check(c.update(&a,t,10).empty(),"qualification required again after speed excursion");
    check(c.update(&a,t,.01).size()==1,"requalify after speed excursion");
    t.ground=false;check(c.update(&a,t,1).empty()&&c.low_speed_seconds()==0,"airborne resets");
    c.reset();warm(c,a,t);t.position={500,-120};check(c.update(&a,t,.01).size()==1,"approach active");
    t.position.y=-80-RunwayAhead::nose+1;check(c.update(&a,t,.1).size()==1,"aircraft nose enters area");
    check(c.update(&a,t,2).size()==1,"retain for exactly two seconds in area");
    check(c.update(&a,t,.01).empty(),"inside clear after two seconds without minimum display duration");
    t.position.y=-120;check(c.update(&a,t,.1).empty(),"no repeated warning while same conflict persists");
    t.position.y=-1000;c.update(&a,t,10);t.position.y=-120;
    check(c.update(&a,t,.1).size()==1,"new encounter after clearing area rearms");
    c.reset();warm(c,a,t);t.position.y=-120;c.update(&a,t,.01);
    check(c.update(&a,t,30).size()==1,"retain for exactly thirty seconds");
    check(c.update(&a,t,.01).empty(),"thirty second timeout");
    check(c.update(&a,t,.1).empty(),"timeout does not retrigger every frame");
    c.reset();warm(c,a,t);t.position={500,0};
    check(c.update(&a,t,.01).empty(),"no initial advisory when already on runway");
    c.reset();warm(c,a,t);t.position={500,-120};t.speed=20;
    a.runways.push_back({"10","28",{0,40},{1000,40},40});
    check(c.update(&a,t,.01).size()==2,"two distinct predicted runways");
    check(c.hits().back().first=="09","closest runway in lower box");
    a.runways.push_back({"11","29",{0,20},{1000,20},40});
    check(c.update(&a,t,.01).size()==2&&c.hits().front().first=="11","only two nearest threats retained");
    a=fixture();a.runways[0].stopway_start=100;a.runways[1].stopway_end=100;
    c.reset();warm(c,a,t);t.position={-60-100-RunwayAhead::nose-50,0};t.heading=90;
    check(c.update(&a,t,.01).size()==1&&std::abs(c.hits()[0].seconds-5)<1e-6,"stopway included before sixty metre margin");
    a.runways[0].stopway_start=0;a.runways[1].stopway_end=0;
    check(c.update(&a,t,.01).empty(),"stopway test clears unextended runway immediately");
    c.reset();warm(c,a,t);t.position={500,-120};c.update(&a,t,.01);t.speed=0;
    check(c.update(&a,t,.01).empty(),"stopping clears a newly activated warning immediately");
    check(c.update(&a,t,30).empty(),"stopping remains clear beyond thirty seconds");
    c.reset();warm(c,a,t);t.heading=0;t.position={500,-120};t.valid=false;
    check(c.update(&a,t,.1).empty()&&c.low_speed_seconds()==0,"missing data invalidates warning");
    t.valid=true;t.speed=std::numeric_limits<double>::quiet_NaN();
    check(c.update(&a,t,.1).empty(),"NaN fails closed");
    c.reset();warm(c,a,t);t.position={500,-120};c.update(&a,t,.1);
    a.icao="OTHER";t.position={500,0};check(c.update(&a,t,.1).empty(),"airport change clears encounter state");
    c.reset();warm(c,a,t);t.position={500,-120};c.update(&a,t,.1);
    t.paused=true;a.icao="PAUSED_CHANGE";
    check(c.update(&a,t,1).empty(),"airport change clears stale threats even while paused");t.paused=false;
    c.reset();warm(c,a,t);t.position={500,-120};
    for(auto& r:a.runways)r.stopway_start=std::numeric_limits<double>::quiet_NaN();
    check(c.update(&a,t,.1).empty(),"invalid runway geometry is skipped");
    std::cout<<"PASS "<<checks<<" runway advisory checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
