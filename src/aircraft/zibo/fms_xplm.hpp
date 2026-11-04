#pragma once
#include "fms_source.hpp"
#include <XPLMDataAccess.h>
#include <XPLMUtilities.h>
#include <array>
#include <map>
namespace oans::zibo {
class DatarefFmsPort final : public FmsSource {
public:
    ~DatarefFmsPort() override {clear(false);}
    bool uses_mcdu_sampling_setting() const override {return false;}
    bool bind() override;
    void bind_activity() override;
    void tick(double now,bool usable) override;
    void clear(bool restore) override;
    void reset_tracking() override {pending_=false;read_at_=0;last_phase_=-1;}
    bool ready() const override;
    const FmsPlan& plan() const override {return plan_;}
    const std::string& status() const override {return status_;}
    unsigned generation() const override {return generation_;}
    void diagnostics(std::ostream&,double,bool) const override;
private:
    std::array<XPLMDataRef,4> endpoints_{};
    XPLMDataRef phase_=nullptr;
    std::map<XPLMCommandRef,std::string> observed_;
    FmsPlan plan_;
    std::string status_="NOT SAMPLED",trigger_;
    bool pending_=false;
    double last_read_=-1,read_at_=0;
    int last_phase_=-1;
    unsigned generation_=0;
    int phase() const;
    void sample(double now,const std::string& trigger);
    static int activity(XPLMCommandRef,XPLMCommandPhase,void*);
};
}
