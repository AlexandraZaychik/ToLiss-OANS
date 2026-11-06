#pragma once
#include "core.hpp"
#include "fms.hpp"
#include <algorithm>
#include <cmath>

namespace oans {
inline bool plan_enroute_phase(const std::string& family,double phase) {
    // Shared policy: ECAM 6 includes climb, cruise and descent.
    return (family=="toliss"||family=="lr")&&phase==6;
}
// Airport-to-airport distance, not route length or remaining FMS distance.
// Great-circle distance also handles the date line and long-haul flights.
inline double airport_distance_nm(double lat,double lon,double other_lat,double other_lon) {
    const double a=lat*pi/180,b=other_lat*pi/180;
    const double s=std::sin((b-a)/2),t=std::sin((other_lon-lon)*pi/360);
    const double h=std::clamp(s*s+std::cos(a)*std::cos(b)*t*t,0.0,1.0);
    return 2*earth*std::atan2(std::sqrt(h),std::sqrt(1-h))/1852;
}
inline bool plan_prefers_destination(bool last_consulted_destination,bool enroute,
                                     double flight_nm,double from_origin_nm) {
    return last_consulted_destination||
        (std::isfinite(flight_nm)&&flight_nm>=0&&
         ((enroute&&flight_nm<300)||
          (flight_nm>350&&std::isfinite(from_origin_nm)&&from_origin_nm>50)));
}
// Keep crew consultation separate from automatic loads and draft UI selections.
class PlanAirportSelection {
    std::string last_consulted_;
public:
    void consult(const std::string& airport) {last_consulted_=airport;}
    void reset() {last_consulted_.clear();}
    const std::string& last_consulted() const {return last_consulted_;}
    std::optional<IndexEntry> select_airborne_open(const std::vector<IndexEntry>& index,const FmsPlan& plan,
                                                   double lat,double lon) const {
        if(auto selected=find_airport(index,last_consulted_))return selected;
        if(plan.observed)if(auto dest=find_airport(index,plan.destination.airport))return dest;
        return nearest(index,lat,lon);
    }
    std::optional<IndexEntry> select(const std::vector<IndexEntry>& index,const FmsPlan& plan,
                                     double lat,double lon,bool enroute) const {
        if(!plan.observed)return std::nullopt;
        auto origin=find_airport(index,plan.origin.airport);
        auto dest=find_airport(index,plan.destination.airport);
        if(dest&&!last_consulted_.empty()&&last_consulted_==dest->icao)return dest;
        if(origin&&dest&&plan_prefers_destination(false,enroute,
            airport_distance_nm(origin->lat,origin->lon,dest->lat,dest->lon),
            airport_distance_nm(lat,lon,origin->lat,origin->lon)))return dest;
        return origin; // Missing endpoints are handled by the host's nearest fallback.
    }
};
}
