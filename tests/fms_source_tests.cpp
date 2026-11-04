#include "fms_source.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
void check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
struct Events {int binds=0,observers=0,reads=0,clears=0,restores=0,resets=0,destroyed=0,logs=0;};
// A direct API source deliberately has no MCDU screen or sampler.
class ApiSource final : public oans::FmsSource {
    Events& events_;
    oans::FmsPlan plan_;
    std::string status_="API IDLE";
    bool bound_=false;
    unsigned generation_=0;
    bool uses_setting_;
public:
    explicit ApiSource(Events& events,bool uses_setting=false):events_(events),uses_setting_(uses_setting) {}
    bool uses_mcdu_sampling_setting() const override {return uses_setting_;}
    ~ApiSource() override {++events_.destroyed;}
    bool bind() override {++events_.binds;return bound_=true;}
    void bind_activity() override {++events_.observers;}
    void tick(double,bool usable) override {
        if(!usable||!bound_)return;
        ++events_.reads;++generation_;status_="API READ";
        plan_.origin.airport="ZUCK";plan_.destination.airport="ZUUU";plan_.observed=true;
        if(log_event)log_event("API READ");
    }
    void clear(bool restore) override {
        ++events_.clears;if(restore)++events_.restores;
        bound_=false;generation_=0;plan_={};status_="API IDLE";
    }
    void reset_tracking() override {++events_.resets;}
    bool ready() const override {return bound_;}
    const oans::FmsPlan& plan() const override {return plan_;}
    const std::string& status() const override {return status_;}
    unsigned generation() const override {return generation_;}
    void diagnostics(std::ostream& out,double,bool) const override {out<<"API SOURCE";}
};
int logs=0;
}
int main() {
    try {
        Events first,second;
        oans::FmsPort port;
        check(!port.bind()&&!port.ready()&&!port.plan().observed,"disconnected state");
        check(!port.sampling_enabled(false)&&!port.sampling_enabled(true),"disconnected source cannot be enabled");
        port.tick(1,true);port.bind_activity();port.reset_tracking();port.clear(true);
        std::ostringstream empty;port.diagnostics(empty,1,true);
        check(empty.str().find("source=NONE")!=std::string::npos,"disconnected diagnostic");
        port.log_event=[](const std::string& text){if(text=="API READ")++logs;};
        port.select(std::make_unique<ApiSource>(first));
        check(port.sampling_enabled(false)&&port.sampling_enabled(true),"direct source is independent of MCDU setting");
        check(port.bind()&&port.ready(),"API source binds without MCDU");
        port.bind_activity();port.tick(2,false);check(first.reads==0,"unusable source does not publish");
        port.tick(3,true);
        check(port.plan().observed&&port.plan().origin.airport=="ZUCK"&&port.generation()==1&&logs==1,"API source supplies common plan and log");
        port.clear(true);port.reset_tracking();
        check(first.restores==1&&first.resets==1&&!port.plan().observed,"clear forwards restoration policy and removes plan");
        port.bind();port.tick(4,true);
        port.select(std::make_unique<ApiSource>(second,true));
        check(!port.sampling_enabled(false)&&port.sampling_enabled(true),"CDU source still obeys MCDU setting after source switch");
        check(first.clears==2&&first.destroyed==1,"switch disconnects and destroys previous source");
        check(!port.ready()&&!port.plan().observed&&port.generation()==0,"switch cannot retain previous flight plan");
        port.bind();port.tick(5,true);
        check(second.reads==1&&logs==3&&port.status()=="API READ","new source receives logger and independent state");
        std::ostringstream api;port.diagnostics(api,5,true);
        check(api.str()=="API SOURCE","source owns its diagnostics");
        port.select(nullptr);
        check(second.clears==1&&second.destroyed==1&&!port.ready()&&port.status()=="NOT CONNECTED","unsupported aircraft detaches source");
        std::cout<<"PASS aircraft-neutral API FMS source, replacement, disconnection and cleanup\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
