#include "runway_ahead.hpp"
#include "test_support.hpp"
#include <algorithm>
#include <iostream>
#include <iomanip>
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
    auto a=fixture();RunwayAhead c;auto t=taxi();
    struct Model {const char* vendor;const char* code;double ref,gear,nose,span;};
    for(const auto& m:std::vector<Model>{
      {"zibo","B738",60,15,45*.3048+4.4,34.4},
      {"lr","A333",97,21.860000610,29.572671814072,60.3},
      {"toliss","A321",54,3.75,20.3862,35.8},
      {"toliss","A339",-6.440000057,-83.269996643,30.0877829594128,64},
      {"toliss","A346",-.680000007,-103.510002136,37.9125846489192,63.45}}){
      scenario(std::string("runway/envelope/")+m.code);
      const auto envelope=aircraft_geometry::envelope_for(m.vendor,m.code,m.ref,m.gear);
      check(!envelope.fallback&&std::abs(envelope.nose-m.nose)<1e-8&&envelope.half_span==m.span/2,"vendor-specific calibrated protection envelope");
      auto airport=fixture();RunwayAhead detector;auto input=taxi();detector.set_envelope(envelope);warm(detector,airport,input);
      input.position={500,-80-envelope.nose-70};const auto hits=detector.update(&airport,input,.1);
      check(hits.size()==1&&std::abs(hits[0].seconds-7)<1e-6,"calibration reaches detector prediction");
      detector.reset();warm(detector,airport,input);
      input.position.x=-60-envelope.half_span+.01;input.position.y=-80-envelope.nose-65;
      check(detector.update(&airport,input,.1).size()==1,"model wing tip intersects runway protection area");
      input.position.x-=.02;check(detector.update(&airport,input,.1).empty(),"model wing tip just outside clears warning");
    }
    scenario("runway/qualification-rearm-pause-stop");
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
    scenario("runway/inside-suppression");
    c.reset();warm(c,a,t);t.position={500,-120};check(c.update(&a,t,.01).size()==1,"approach active");
    t.position.y=-80-RunwayAhead::nose+1;check(c.update(&a,t,.1).size()==1,"aircraft nose enters area");
    check(c.update(&a,t,2).size()==1,"retain for exactly two seconds in area");
    check(c.update(&a,t,.01).empty(),"inside clear after two seconds without minimum display duration");
    t.position.y=-120;check(c.update(&a,t,.1).empty(),"no repeated warning while same conflict persists");
    t.position.y=-1000;c.update(&a,t,10);t.position.y=-120;
    check(c.update(&a,t,.1).size()==1,"new encounter after clearing area rearms");
    scenario("runway/timeout-suppression");
    c.reset();warm(c,a,t);t.position.y=-120;c.update(&a,t,.01);
    check(c.update(&a,t,30).size()==1,"retain for exactly thirty seconds");
    check(c.update(&a,t,.01).empty(),"thirty second timeout");
    check(c.update(&a,t,.1).empty(),"timeout does not retrigger every frame");
    scenario("runway/already-on-runway");
    c.reset();warm(c,a,t);t.position={500,0};
    check(c.update(&a,t,.01).empty(),"no initial advisory when already on runway");
    scenario("runway/nearest-two-threats");
    c.reset();warm(c,a,t);t.position={500,-120};t.speed=20;
    a.runways.push_back({"10","28",{0,40},{1000,40},40});
    check(c.update(&a,t,.01).size()==2,"two distinct predicted runways");
    check(c.hits().back().first=="09","closest runway in lower box");
    a.runways.push_back({"11","29",{0,20},{1000,20},40});
    check(c.update(&a,t,.01).size()==2&&c.hits().front().first=="11","only two nearest threats retained");
    a=fixture();a.runways[0].stopway_start=100;a.runways[1].stopway_end=100;
    scenario("runway/stopway-protection");
    c.reset();warm(c,a,t);t.position={-60-100-RunwayAhead::nose-50,0};t.heading=90;
    check(c.update(&a,t,.01).size()==1&&std::abs(c.hits()[0].seconds-5)<1e-6,"stopway included before sixty metre margin");
    a.runways[0].stopway_start=0;a.runways[1].stopway_end=0;
    check(c.update(&a,t,.01).empty(),"stopway test clears unextended runway immediately");
    scenario("runway/invalid-provider");
    c.reset();warm(c,a,t);t.heading=0;t.position={500,-120};t.valid=false;
    check(c.update(&a,t,.1).empty()&&c.low_speed_seconds()==0,"missing data invalidates warning");
    t.valid=true;t.speed=std::numeric_limits<double>::quiet_NaN();
    check(c.update(&a,t,.1).empty(),"NaN fails closed");
    scenario("runway/airport-change");
    c.reset();warm(c,a,t);t.position={500,-120};c.update(&a,t,.1);
    a.icao="OTHER";t.position={500,0};check(c.update(&a,t,.1).empty(),"airport change clears encounter state");
    scenario("runway/paused-airport-change");
    c.reset();warm(c,a,t);t.position={500,-120};c.update(&a,t,.1);
    t.paused=true;a.icao="PAUSED_CHANGE";
    check(c.update(&a,t,1).empty(),"airport change clears stale threats even while paused");t.paused=false;
    scenario("runway/invalid-geometry");
    c.reset();warm(c,a,t);t.position={500,-120};
    for(auto& r:a.runways)r.stopway_start=std::numeric_limits<double>::quiet_NaN();
    check(c.update(&a,t,.1).empty(),"invalid runway geometry is skipped");
    std::cout<<"PASS "<<checks<<" runway advisory checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
