#pragma once
#include "fms_light_capture.hpp"
#include "test_support.hpp"

inline void test_fms_lights() {
    using oans::FmsLightCapture;
    scenario("toliss/fms/automatic-phase-gate-and-latch");
    FmsLightCapture lights;
    lights.poll(0,{{1,1}});
    verify(lights.request(0,true)==-1,"initial ON is a baseline, not an edge");
    verify(!lights.poll_due(4.9)&&lights.poll_due(5),"poll only every five seconds");
    lights.poll(5,{{0,0}});lights.poll(10,{{1,1}});
    verify(lights.request(10,false)==-1,"unavailable sampler preserves both events");
    verify(lights.request(10,true)==0&&lights.attempt()==1,"beacon rising edge starts first attempt");
    verify(lights.request(10,true)==-1,"never run overlapping transactions");
    lights.finish(11,false);
    verify(lights.request(12,true)==1,"landing event is independent of beacon retry");
    lights.finish(13,true);
    verify(lights.request(15.9,true)==-1,"retry waits five seconds after completion");
    verify(lights.request(16,true)==0&&lights.attempt()==2,"first retry");lights.finish(17,false);
    verify(lights.request(22,true)==0&&lights.attempt()==3,"second retry");lights.finish(23,false);
    lights.poll(30,{{1,1}});
    verify(lights.request(30,true)==-1,"three failed attempts exhaust session; steady ON never retriggers");
    lights.poll(35,{{0,1}});lights.poll(40,{{1,1}});
    verify(lights.request(40,true)==0&&lights.attempt()==1,"new edge gets a new retry budget");
    lights.poll(45,{{0,1}});lights.poll(50,{{1,1}});lights.finish(51,true);
    verify(lights.request(51,true)==0&&lights.attempt()==1,"old result does not consume newer edge");lights.finish(52,true);
    lights.poll(55,{{-1,-1}});lights.poll(60,{{1,1}});
    verify(lights.request(60,true)==-1,"missing data cannot invent an OFF->ON edge");
    lights=FmsLightCapture{};lights.poll(65,{{1,1}});
    verify(lights.request(65,true)==-1,"reset discards all old edges");
    verify(oans::fms_landing_pair_state(0,0)==0&&oans::fms_landing_pair_state(1,1)==1,
           "landing pair requires both OFF or both ON");
    verify(oans::fms_landing_pair_state(1,0)==oans::fms_light_mixed&&
           oans::fms_landing_pair_state(0,1)==oans::fms_light_mixed,"either mixed position is intermediate");
    lights=FmsLightCapture{};
    auto pair=[&](double now,float left,float right){lights.poll(now,{{0,oans::fms_landing_pair_state(left,right)}});};
    pair(0,0,0);pair(5,1,0);
    verify(lights.request(5,true)==-1,"first landing switch alone cannot trigger");
    pair(10,1,1);
    verify(lights.request(10,true)==1,"second landing switch completes OFF->ON");lights.finish(11,true);
    pair(15,0,1);pair(20,1,1);
    verify(lights.request(20,true)==-1,"one switch OFF then ON does not rearm the pair");
    pair(25,0,1);pair(30,0,0);pair(35,0,1);pair(40,1,1);
    verify(lights.request(40,true)==1,"both switches OFF rearms detection in either switching order");
    lights=FmsLightCapture{};pair(45,1,0);pair(50,1,1);
    verify(lights.request(50,true)==-1,"initial mixed pair cannot invent an OFF baseline");
}
