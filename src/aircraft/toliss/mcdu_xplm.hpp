#pragma once
#include "mcdu.hpp"
#include "fms_source.hpp"
#include <XPLMDataAccess.h>
#include <XPLMUtilities.h>
#include <vector>
#include <array>

namespace oans::toliss {
class McduFmsPort final : public FmsSource {
public:
    FmsSampler sampler{false};
    FmsAutoCapture automatic;
    bool bind() override;
    void bind_activity() override;
    const std::string& major_page(int unit) const {return major_pages_.at(unit-1);}
    void reset_tracking() override {major_pages_={"DATA","DATA"};mcdu2_operated_=false;}
    void tick(double now,bool usable) override;
    void clear(bool restore) override;
    bool ready() const override {return !text_.empty();}
    const FmsPlan& plan() const override {return sampler.plan();}
    const std::string& status() const override {return sampler.status();}
    unsigned generation() const override {return sampler.generation();}
    void diagnostics(std::ostream&,double now,bool enabled) const override;
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
    struct InitRead {unsigned fields=0,retry=0;double due=0;};
    std::array<InitRead,2> init_reads_{};
    McduScreen read() const;
    McduScreen read_init(int unit) const;
    void capture_init(double now,bool usable);
    void refresh_phase(double now);
    void send(const std::string& suffix);
    void finish_detection(const std::string& result);
    static int activity(XPLMCommandRef,XPLMCommandPhase,void*);
};
}
