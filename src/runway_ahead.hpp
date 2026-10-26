#pragma once
#include "core.hpp"
#include "aircraft_geometry.hpp"
#include <cmath>

namespace oans {
struct RunwayAheadInput {
    Vec2 position;
    double speed=0, heading=0; // m/s, true degrees; constant-heading taxi prediction
    bool ground=false, valid=true, paused=false;
};
struct RunwayAheadHit {
    std::string first, second;
    double seconds=0, distance=0;
};
// Resolve entered end identifiers to actual physical runway pairs. Invalid
// input throws; reciprocal ends are deduplicated and at most two pairs allowed.
std::vector<RunwayAheadHit> test_runways(const Airport& airport,const std::string& text);
// Presentation only: put thresholds on their aircraft-relative left/right
// sides using true heading and runway geometry. Preserve threat row ordering.
std::vector<RunwayAheadHit> orient_runway_advisories(const Airport* airport,
    const std::vector<RunwayAheadHit>& hits,double true_heading);
// A350 RWYA's separating-axis protection concept, with shared A320/A20N envelope and
// the supplied ANF timing rules. No simulator or rendering dependencies.
class RunwayAhead {
    struct State {bool active=false, inhibited=false, inside=false;double age=0, inside_age=0;};
    std::map<std::string,State> states_;
    std::string airport_;
    double low_speed_=0, clock_=0;
    std::vector<RunwayAheadHit> hits_;
public:
    // Aircraft dimensions are maintained separately from advisory rules.
    static constexpr double fuselage_length=aircraft_geometry::fuselage_length,span=aircraft_geometry::span;
    static constexpr double nose=aircraft_geometry::nose,tail=aircraft_geometry::tail;
    static constexpr double half_span=aircraft_geometry::half_span;
    static constexpr double margin=60,horizon=7;
    void reset();
    const std::vector<RunwayAheadHit>& update(const Airport* airport,const RunwayAheadInput& input,double dt);
    const std::vector<RunwayAheadHit>& hits() const {return hits_;}
    double low_speed_seconds() const {return low_speed_;}
    bool blink() const {return std::fmod(clock_,1.0)<.5;}
};
}
