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
    virtual bool bind()=0;
    virtual void bind_activity()=0;
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
    void bind_activity() {if(source_)source_->bind_activity();}
    void tick(double now,bool usable) {if(source_)source_->tick(now,usable);}
    void clear(bool restore) {if(source_)source_->clear(restore);}
    void reset_tracking() {if(source_)source_->reset_tracking();}
    bool ready() const {return source_&&source_->ready();}
    const FmsPlan& plan() const {return source_?source_->plan():empty_;}
    const std::string& status() const {return source_?source_->status():disconnected_;}
    unsigned generation() const {return source_?source_->generation():0;}
    void diagnostics(std::ostream& out,double now,bool enabled) const {
        if(source_)source_->diagnostics(out,now,enabled);
        else out<<"FMS source=NONE enabled="<<enabled<<" bound=0\n";
    }
};
}
