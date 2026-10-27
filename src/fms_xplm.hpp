#pragma once
#include "fms.hpp"
#include <XPLMDataAccess.h>
#include <XPLMUtilities.h>
#include <vector>
#include <array>

namespace oans {
class McduFmsPort {
public:
    FmsSampler sampler{false};
    FmsAutoCapture automatic;
    void (*log_event)(const std::string&)=nullptr;
    bool bind();
    void bind_activity();
    const std::string& major_page(int unit) const {return major_pages_.at(unit-1);}
    void reset_major_pages() {major_pages_={"DATA","DATA"};mcdu2_operated_=false;}
    void tick(double now,bool usable);
    void clear(bool restore);
    bool ready() const {return !text_.empty();}
private:
    std::map<std::string,XPLMDataRef> text_;
    std::map<std::string,XPLMCommandRef> pages_;
    struct ObservedKey {int unit;std::string major,key;};
    std::map<XPLMCommandRef,ObservedKey> observed_;
    std::array<std::string,2> major_pages_{{"DATA","DATA"}};
    bool issuing_=false;
    bool mcdu2_operated_=false;
    double next_read_=0;
    unsigned generation_seen_=0;
    unsigned auto_attempts_=0;
    std::string trigger_source_,detection_source_;
    McduScreen read() const;
    void send(const std::string& suffix);
    void finish_detection(const std::string& result);
    static int activity(XPLMCommandRef,XPLMCommandPhase,void*);
};
}
