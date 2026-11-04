#include "route_plan.hpp"
#include <iostream>
#include <stdexcept>
using namespace oans;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main() {
    Airport a;
    a.icao="ZTST";
    a.nodes={{1,{0,0}},{2,{100,0}},{3,{200,0}},{4,{300,0}},{5,{400,0}},{6,{500,0}},{7,{600,0}}};
    a.edges={{1,2,false,false,"L1"},{2,3,false,false,"A"},{3,4,false,false,"A1"},
             {4,5,false,false,"B"},{5,6,false,false,"T1"},{6,7,false,false,"DEAD"}};
    a.stands={{{-10,0},0,"Ramp Start 512"},{{510,0},0,"315"}};
    a.runways={{"02L","20R",{300,-500},{300,500},45},
               {"02R","20L",{200,-500},{200,500},45}};
    auto r=make_route_plan(a,"512 l1 a a1 rw02l");
    check(bool(r),"departure example");
    check(r.points.size()==5&&r.points.front().x==-10&&r.points.back().x==300,"nearest stand node and ordered route");
    check(std::abs(r.metres-310)<.001,"departure distance");
    r=make_route_plan(a,"rw02r-a1-b-t1-315");
    check(bool(r)&&r.points.front().x==200&&r.points.back().x==510,"arrival example");
    check(bool(make_route_plan(a,"512,l1;A/A1>RWY02L")),"mixed separators and case");
    check(bool(make_route_plan(a,"[ztst]512 L1 A A1 RW02L")),"leading ICAO tag");
    check(bool(make_route_plan(a,"512 L1 A A1 RW02L [ZTST]")),"trailing ICAO tag");
    check(!make_route_plan(a,"[ZUUU] 512 L1 A A1 RW02L"),"mismatched airport rejected");
    check(!parse_route_input("[ZTST").error.empty(),"unclosed airport tag");
    check(!parse_route_input("[ZTST][ZUUU] A").error.empty(),"multiple tags rejected");
    check(!parse_route_input("[ABC] A").error.empty(),"short ICAO rejected");
    check(!parse_route_input("A ]ZTST[").error.empty(),"reversed brackets rejected");
    auto fuzzy=a;fuzzy.stands.front().name="Gate512Heavy";
    auto matched=make_route_plan(fuzzy,"512 L1 A A1 RW02L [ZTST]");
    check(bool(matched)&&matched.start_stand=="Gate512Heavy","substring stand fallback");
    fuzzy.stands.push_back({{-20,0},0,"Other512Wide"});
    check(!make_route_plan(fuzzy,"512 L1 A A1 RW02L"),"ambiguous substring rejected");
    fuzzy.stands.push_back({{-10,0},0,"512"});
    check(bool(make_route_plan(fuzzy,"512 L1 A A1 RW02L")),"exact stand wins over substring candidates");
    check(!make_route_plan(a,"512 L1 MISSING RW02L"),"unknown taxiway rejected");
    check(!make_route_plan(a,"512 L1 T1 315"),"disconnected names rejected");
    a.edges[2].oneway=true;
    check(!make_route_plan(a,"315 T1 B A1 RW02R"),"reverse one-way rejected");
    a.edges[2].oneway=false;
    check(bool(make_route_plan(a,"315 T1 B A1 RW02R")),"reverse two-way accepted");
    check(!make_route_plan(a,"512 L1 RW99"),"unknown runway rejected");
    check(!make_route_plan(a,"512"),"stand alone rejected");
    check(!make_route_plan(a,"512 L1 RW02L"),"taxiway does not reach runway");
    a.edges.push_back({90,91,false,false,"BROKEN"});
    check(!make_route_plan(a,"512 BROKEN RW02L"),"missing nodes rejected");
    a.nodes[8]={90,50};a.edges.push_back({1,8,false,false,"L1"});
    check(bool(make_route_plan(a,"512 L1 A A1 RW02L")),"branch resolves via connectivity");
    a.stands.push_back({{-20,0},0,"Stand 512"});
    check(!make_route_plan(a,"512 L1 A A1 RW02L"),"ambiguous stand rejected");
    Airport projection;projection.icao="ZPRJ";
    projection.nodes={{1,{0,0}},{2,{100,0}},{3,{200,100}}};
    projection.edges={{1,2,false,false,"C"},{2,3,false,false,"T1"}};
    projection.stands={{{170,30},0,"25"},{{140,0},0,"26"},{{140,100},0,"27"}};
    r=make_route_plan(projection,"C T1 25");
    check(bool(r)&&r.points.size()==4,"arrival splits the final taxiway edge at the stand projection");
    const auto join=r.points[r.points.size()-2];
    check(length(join-Vec2{150,50})<1e-9,"diagonal taxiway uses the closest interior point");
    check(std::abs(dot(r.points.back()-join,Vec2{100,100}))<1e-8,"stand connector is perpendicular to the taxiway");
    check(std::abs(r.metres-(100+std::sqrt(5000.0)+std::sqrt(800.0)))<1e-8,"arrival distance includes the partial edge and direct connector");
    const auto arrival=r.points;
    r=make_route_plan(projection,"25 T1 C");
    check(bool(r)&&r.points.size()==arrival.size(),"departure uses the same interior stand attachment");
    for(size_t i=0;i<arrival.size();++i)check(length(r.points[i]-arrival[arrival.size()-1-i])<1e-9,"reverse route has the same geometry");
    r=make_route_plan(projection,"26 T1 27");
    check(bool(r)&&r.points.size()==4&&length(r.points[1]-Vec2{120,20})<1e-9&&length(r.points[2]-Vec2{170,70})<1e-9,
          "two stands can split the same edge at different positions");
    projection.edges[1].oneway=true;
    check(bool(make_route_plan(projection,"C T1 25")),"partial arrival respects forward one-way direction");
    check(!make_route_plan(projection,"25 T1 C"),"partial departure cannot reverse a one-way edge");
    check(!make_route_plan(projection,"27 T1 26"),"same-edge stand attachments cannot reverse one-way direction");
    projection.edges[1].oneway=false;
    projection.edges.push_back({-1,1,false,false,"BROKEN"});
    check(!make_route_plan(projection,"25 T1 BROKEN"),"virtual stand node cannot make a missing source node valid");
    projection.edges.pop_back();
    projection.nodes[4]={160,20};projection.nodes[5]={180,20};
    projection.edges.push_back({4,5,false,false,"T1"});
    check(!make_route_plan(projection,"C T1 25"),"closest disconnected edge does not invent a connection to the route");
    check(projection.nodes.size()==5&&projection.edges.size()==3,"virtual splits do not mutate airport geometry");
    auto strip=route_ribbon({{0,0},{100,0},{100,100}});
    check(strip.size()==2,"ribbon segment count");
    check(std::abs(strip[0][0].y-10)<1e-9&&std::abs(strip[0][1].y+10)<1e-9,"10 metre half width / 20 metre total width");
    check(length(strip[0][3]-strip[1][0])<1e-9,"continuous corner");
    std::cout<<"Route plan: example routes, connectivity, direction, endpoints and ribbon passed\n";
}
