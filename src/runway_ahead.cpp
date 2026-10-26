#include "runway_ahead.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <limits>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace oans {
std::vector<RunwayAheadHit> orient_runway_advisories(const Airport* airport,
    const std::vector<RunwayAheadHit>& hits,double true_heading) {
    auto result=hits;
    if(!airport||!std::isfinite(true_heading))return result;
    const double angle=std::remainder(true_heading,360.0)*pi/180;
    const Vec2 right{std::cos(angle),-std::sin(angle)};
    for(auto& hit:result)for(const auto& r:airport->runways) {
        const bool forward=hit.first==r.name&&hit.second==r.reciprocal;
        if(!forward&&!(hit.first==r.reciprocal&&hit.second==r.name))continue;
        const auto direction=forward?r.end-r.start:r.start-r.end;
        const double lateral=dot(direction,right);
        // Exactly fore/aft has no left/right distinction; keep its input order.
        if(std::isfinite(lateral)&&lateral<-1e-6)std::swap(hit.first,hit.second);
        break;
    }
    return result;
}

std::vector<RunwayAheadHit> test_runways(const Airport& airport,const std::string& text) {
    auto normalize=[](std::string s) {
        for(char& c:s)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if(s.rfind("RWY",0)==0)s=s.substr(3);
        char side=0;if(!s.empty()&&(s.back()=='L'||s.back()=='C'||s.back()=='R')) {side=s.back();s.pop_back();}
        if(s.empty()||s.size()>2||!std::all_of(s.begin(),s.end(),[](unsigned char c){return std::isdigit(c);}))return std::string{};
        const int n=std::stoi(s);if(n<1||n>36)return std::string{};
        return (n<10?"0":"")+std::to_string(n)+(side?std::string(1,side):"");
    };
    std::string input=text;
    for(char& c:input)if(c==','||c=='/'||c=='-')c=' ';
    std::istringstream stream(input);std::string word;std::vector<RunwayAheadHit> hits;std::set<std::string> seen;
    while(stream>>word) {
        const auto wanted=normalize(word);if(wanted.empty())throw std::invalid_argument("INVALID RUNWAY: "+word);
        const Runway* found=nullptr;
        for(const auto& r:airport.runways)if(normalize(r.name)==wanted||normalize(r.reciprocal)==wanted) {found=&r;break;}
        if(!found)throw std::invalid_argument("RUNWAY "+wanted+" NOT IN "+airport.icao);
        const auto key=std::min(found->name,found->reciprocal)+"/"+std::max(found->name,found->reciprocal);
        if(seen.insert(key).second)hits.push_back({found->name,found->reciprocal,0,0});
    }
    if(hits.empty())throw std::invalid_argument("ENTER A RUNWAY END, E.G. 02L");
    if(hits.size()>2)throw std::invalid_argument("AT MOST TWO PHYSICAL RUNWAYS");
    return hits;
}
namespace {
// Continuous SAT: intersect time intervals on the two rectangles' axes.
// This is the translating form of A350 RWYA.overlap's separating-axis test.
double entry_time(const Runway& r,Vec2 p,Vec2 forward,double speed) {
    const auto u=r.axis();const Vec2 n{u.y,-u.x},right{forward.y,-forward.x};
    const double lo=-r.stopway_start-RunwayAhead::margin;
    const double hi=r.length()+r.stopway_end+RunwayAhead::margin;
    const auto center=r.start+u*((lo+hi)*.5);
    const double along=(hi-lo)*.5,across=r.width*.5+RunwayAhead::margin;
    const auto ac=p+forward*((RunwayAhead::nose-RunwayAhead::tail)*.5);
    double enter=0,leave=RunwayAhead::horizon;
    for(auto axis:{u,n,forward,right}) {
        const double radius=along*std::abs(dot(u,axis))+across*std::abs(dot(n,axis))+
            (RunwayAhead::nose+RunwayAhead::tail)*.5*std::abs(dot(forward,axis))+
            RunwayAhead::half_span*std::abs(dot(right,axis));
        const double d=dot(ac-center,axis),v=speed*dot(forward,axis);
        if(std::abs(v)<1e-9) {if(std::abs(d)>radius)return INFINITY;}
        else {
            double a=(-radius-d)/v,b=(radius-d)/v;if(a>b)std::swap(a,b);
            enter=std::max(enter,a);leave=std::min(leave,b);
            if(enter>leave+1e-9)return INFINITY;
        }
    }
    return enter<=leave+1e-9?enter:INFINITY;
}
}
void RunwayAhead::reset() {states_.clear();airport_.clear();low_speed_=clock_=0;hits_.clear();}
const std::vector<RunwayAheadHit>& RunwayAhead::update(const Airport* a,const RunwayAheadInput& in,double dt) {
    const bool valid=in.valid&&std::isfinite(in.speed)&&in.speed>=0&&std::isfinite(in.heading)&&
        std::isfinite(in.position.x)&&std::isfinite(in.position.y)&&std::isfinite(dt)&&dt>=0;
    if(!valid||!in.ground||in.speed>=40*kt) {reset();return hits_;}
    const auto airport=a?a->icao:std::string{};
    if(airport_!=airport) {states_.clear();hits_.clear();airport_=airport;}
    if(in.paused)return hits_;
    clock_+=dt;low_speed_+=dt;hits_.clear();
    if(!a)return hits_;
    const double h=in.heading*pi/180;const Vec2 forward{std::sin(h),std::cos(h)};
    std::set<std::string> seen;
    for(const auto& r:a->runways) {
        const auto key=std::min(r.name,r.reciprocal)+"/"+std::max(r.name,r.reciprocal);
        if(!seen.insert(key).second||!std::isfinite(r.length())||r.length()<1||!std::isfinite(r.width)||r.width<=0||
           !std::isfinite(r.stopway_start)||!std::isfinite(r.stopway_end)||r.stopway_start<0||r.stopway_end<0)continue;
        auto& s=states_[key];
        const double eta=entry_time(r,in.position,forward,in.speed);
        const bool predicted=std::isfinite(eta),inside=predicted&&eta<=1e-9;
        if(inside) {s.inside_age=s.inside?s.inside_age+dt:0;}
        else s.inside_age=0;
        s.inside=inside;
        if(s.active)s.age+=dt;
        // Airbus Flight Operations Support & Services, Getting to Grips with
        // Surveillance, Issue I (May 2009), section 5.1.2.2, printed p. 5-7:
        // "The APPROACHING RUNWAY advisory is displayed for at least 10 s."
        // https://www.theairlinepilots.com/forumarchive/com-nav-surv/surveillance.pdf#page=136
        // Minimum display duration disabled by request; preserve the old rule:
        // if(s.active&&s.age>=10) { ... }
        // This does NOT disable the separate low-speed >10 s qualification.
        // Stopping still clears immediately, independently of display age.
        if(!predicted)s.inhibited=false;
        if(s.active) {
            if(!predicted||in.speed<.5) {s.active=false;s.age=0;}
            else if(s.inside_age>2||s.age>30) {s.active=false;s.inhibited=true;}
        }
        if(!s.active&&!s.inhibited&&!inside&&predicted&&in.speed>=.5&&low_speed_>10) {s.active=true;s.age=0;}
        if(s.active) {
            const auto sd=r.station(in.position);
            const double along=std::max({-r.stopway_start-margin-sd.x,sd.x-r.length()-r.stopway_end-margin,0.0});
            const double cross=std::max(0.0,std::abs(sd.y)-r.width*.5-margin);
            hits_.push_back({r.name,r.reciprocal,predicted?eta:0,std::hypot(along,cross)});
        }
    }
    std::sort(hits_.begin(),hits_.end(),[](const auto& x,const auto& y){
        if(x.distance!=y.distance)return x.distance<y.distance;
        return x.first<y.first;
    });
    if(hits_.size()>2)hits_.resize(2);
    std::reverse(hits_.begin(),hits_.end()); // closest runway in the bottom box
    return hits_;
}
}
