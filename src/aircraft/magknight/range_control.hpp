#pragma once
#include <cmath>

namespace oans::magknight {
// Applies only when opening OANS; closing and visibility keep their own rules.
inline bool can_open(bool ground_available,bool ground,double physical_range_nm) {
    return ground_available&&ground&&std::isfinite(physical_range_nm)&&
        physical_range_nm>0&&physical_range_nm<=5;
}
// While the ground OANS is active, physical detents 0 and 2 signal a
// zoom action. Return the selector to 1 so the next action is observable.
struct RangeControl {
    bool active=false;
    int previous=-1;
    struct Change { bool open=false, close=false, zoom_in=false, zoom_out=false, reset_selector=false; };
    Change update(bool valid, bool ground, int selector, bool at_max) {
        if (!valid) { previous=-1; return {}; }
        Change c;
        if (!ground) { if(active)c.close=true; active=false; previous=selector; return c; }
        if (!active) {
            if(selector>=0 && selector<=1) { active=true;c.open=true;c.reset_selector=selector==0; }
        } else if (selector==0 && previous!=0) { c.zoom_in=true;c.reset_selector=true; }
        else if (selector==2 && previous!=2) {
            if(at_max) { active=false;c.close=true; }
            else { c.zoom_out=true;c.reset_selector=true; }
        } else if(selector>2 || selector<0) { active=false;c.close=true; }
        previous=selector;
        return c;
    }
    void reset() { active=false; previous=-1; }
};
}
