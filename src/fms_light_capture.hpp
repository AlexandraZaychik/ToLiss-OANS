#pragma once
#include <array>
#include <cmath>

namespace oans {
// Mixed switches retain the last confirmed pair state; unavailable data resets it.
inline constexpr int fms_light_mixed=-2;
inline int fms_landing_pair_state(float left,float right) {
    if(!std::isfinite(left)||!std::isfinite(right))return -1;
    if((left>0)!=(right>0))return fms_light_mixed;
    return left>0?1:0;
}
// Separate edge-triggered sessions; an attempt plus at most two retries.
class FmsLightCapture {
    struct Light {bool known=false,on=false,pending=false;unsigned serial=0,attempts=0;double due=0;};
    std::array<Light,2> lights_{};
    double next_poll_=0;
    int active_=-1;
    unsigned active_serial_=0;
public:
    bool poll_due(double now) const {return now>=next_poll_;}
    void poll(double now,const std::array<int,2>& states) {
        next_poll_=now+5;
        for(unsigned i=0;i<lights_.size();++i) {
            auto& light=lights_[i];
            if(states[i]==fms_light_mixed)continue;
            if(states[i]<0) {light.known=false;continue;}
            const bool on=states[i]!=0;
            if(light.known&&!light.on&&on) {
                light.pending=true;light.attempts=0;light.due=now;++light.serial;
            }
            light.known=true;light.on=on;
        }
    }
    int request(double now,bool available) {
        if(!available||active_>=0)return -1;
        for(unsigned i=0;i<lights_.size();++i) {
            auto& light=lights_[i];
            if(!light.pending||now<light.due)continue;
            active_=static_cast<int>(i);active_serial_=light.serial;++light.attempts;
            return active_;
        }
        return -1;
    }
    unsigned attempt() const {return active_<0?0:lights_[active_].attempts;}
    void finish(double now,bool success) {
        if(active_<0)return;
        auto& light=lights_[active_];
        if(light.serial==active_serial_) {
            light.pending=!success&&light.attempts<3;light.due=now+5;
        }
        active_=-1;
    }
};
}
