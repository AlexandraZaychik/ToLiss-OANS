#pragma once
#include "fms.hpp"
#include <memory>
#include <ostream>

namespace oans {
// Aircraft-neutral flight-plan source. A backend may use an API or its own CDU.
class FmsSource {
public:
    virtual ~FmsSource()=default;
    void (*log_event)(const std::string&)=nullptr;
    // CDU automation follows the user's sampling switch; direct sources may opt out.
    virtual bool uses_mcdu_sampling_setting() const {return true;}
    virtual void set_light_mode(bool) {}
    virtual bool bind()=0;
    virtual void bind_activity()=0;
    // 不会主动操作 MCDU2、翻页并产生响声的只读检测，在所有挡位都有。
    // Visible INIT FROM/TO and ALTN reads remain available in [0], [1] and [2].
    virtual void tick_direct(double,bool) {}
    virtual void tick(double now,bool usable)=0;
    virtual void clear(bool restore)=0;
    virtual void reset_tracking()=0;
    virtual bool ready() const=0;
    virtual const FmsPlan& plan() const=0;
    virtual const std::string& status() const=0;
    virtual unsigned generation() const=0;
    virtual void diagnostics(std::ostream&,double now,bool enabled) const=0;
};
// Stable owner used by the plugin; replacing a backend discards its old handles.
class FmsPort {
    std::unique_ptr<FmsSource> source_;
    FmsPlan empty_;
    const std::string disconnected_="NOT CONNECTED";
public:
    void (*log_event)(const std::string&)=nullptr;
    void select(std::unique_ptr<FmsSource> source) {
        clear(false);source_=std::move(source);
        if(source_)source_->log_event=log_event;
    }
    bool bind() {return source_&&source_->bind();}
    void set_light_mode(bool enabled) {if(source_)source_->set_light_mode(enabled);}
    void bind_activity() {if(source_)source_->bind_activity();}
    void tick_direct(double now,bool usable) {if(source_)source_->tick_direct(now,usable);}
    void tick(double now,bool usable) {if(source_)source_->tick(now,usable);}
    void clear(bool restore) {if(source_)source_->clear(restore);}
    void reset_tracking() {if(source_)source_->reset_tracking();}
    bool ready() const {return source_&&source_->ready();}
    bool uses_mcdu_sampling_setting() const {return !source_||source_->uses_mcdu_sampling_setting();}
    bool sampling_enabled(bool mcdu_enabled) const {return source_&&(mcdu_enabled||!uses_mcdu_sampling_setting());}
    const FmsPlan& plan() const {return source_?source_->plan():empty_;}
    const std::string& status() const {return source_?source_->status():disconnected_;}
    unsigned generation() const {return source_?source_->generation():0;}
    void diagnostics(std::ostream& out,double now,bool enabled) const {
        if(source_)source_->diagnostics(out,now,enabled);
        else out<<"FMS source=NONE enabled="<<enabled<<" bound=0\n";
    }
};
}
