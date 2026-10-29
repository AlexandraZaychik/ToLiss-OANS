#pragma once
#include <map>
#include <string>

namespace oans {
struct McduScreen {
    std::map<std::string,std::string> fields; // names without AirbusFBW/MCDU2
    std::string line(const std::string& stem,const std::string& colours="wgbamsy") const;
    bool contains(const std::string& text) const;
    bool scratchpad_empty() const;
    std::string plan_key() const;
};
enum class McduPage { unknown, fpln, dir, fuel, ident, data, init_a, init_b, radnav, menu, sec, perf, prog, atc, airport };
McduPage mcdu_page(const McduScreen& s);
const char* mcdu_page_name(McduPage p);
struct FmsEndpoint {std::string airport,runway;};
struct FmsPlan {
    FmsEndpoint origin,destination;
    std::string alternate;
    std::string reason="NOT SAMPLED";
    bool observed=false;
};
FmsPlan parse_fpln(const McduScreen& s);
// Accept only a FUEL PRED page whose first row matches the sampled destination.
bool parse_fuel_pred(const McduScreen& s,FmsPlan& plan);
bool airport_outside_active_plan(const FmsPlan& plan,const std::string& airport);

// One automatic capture session per aircraft/port reset. Crew-triggered
// transactions remain independent of phase gating and the completion latch.
class FmsAutoCapture {
public:
    bool phase_due(double now) const {return now>=next_phase_;}
    void update_phase(double now,int phase);
    bool request_due(double now,bool usable,bool busy);
    bool capture_allowed() const {return !complete_&&phase_>=2&&phase_<5;}
    void observe(const FmsPlan& plan);
    int phase() const {return phase_;}
    unsigned confirmations() const {return confirmations_;}
    bool complete() const {return complete_;}
private:
    int phase_=0;
    double next_phase_=0,next_attempt_=0;
    unsigned confirmations_=0;
    bool complete_=false;
};

// Nonblocking transaction. The caller reads one simulator-thread screen per
// tick and executes the returned MCDU2 page command once. No aircraft writes.
class FmsSampler {
public:
    explicit FmsSampler(bool initial_sample=true):requested_(initial_sample) {}
    std::string tick(double now,const McduScreen& screen,bool usable=true,McduPage restore_page=McduPage::unknown);
    void activity(double now,bool request_sample=true); // all input defers; only selected keys request
    void reset(bool initial_sample=true);
    void request() {if(!requested_by_crew_){requested_=true;next_=0;}}
    void stop_automatic_capture();
    void set_flight_phase(int phase) {flight_phase_=phase;}
    // Restore when disabling/stopping, while aircraft handles are still valid.
    // The simulator port supplies the tracked major page; standalone callers
    // without a parent hint retain screen-based restoration / F-PLN fallback.
    std::string cancel();
    const FmsPlan& plan() const {return plan_;}
    const FmsPlan& sampled_plan() const {return pending_;} // Raw endpoints before cached halves / phase protection.
    const std::string& status() const {return status_;}
    bool busy() const {return stage_!=Stage::idle;}
    bool wants_screen(double now) const {return busy()||(requested_&&now>=next_&&now-last_activity_>=crew_idle_seconds);}
    double age(double now) const {return sampled_>=0?now-sampled_:-1;}
    unsigned generation() const {return generation_;}
    bool crew_triggered() const {return sampling_from_crew_;}
    bool complete_pair() const {return complete_pair_;}
    unsigned crew_retry() const {return sampling_retry_;}
    McduPage saved_page() const {return saved_;}
private:
    static constexpr double crew_idle_seconds=5;
    static constexpr unsigned max_crew_retries=1;
    enum class Stage {idle, detour, fpln, stable, fuel, fuel_stable, verify, verify_stable, restore, restore_wait, ident_wait, init_b_wait};
    Stage stage_=Stage::idle;
    McduPage saved_=McduPage::unknown;
    FmsPlan plan_,pending_;
    bool pending_valid_=false;
    bool complete_pair_=false;
    int flight_phase_=0;
    bool requested_=true; // Initial sample; subsequent samples require a request or crew input.
    bool requested_by_crew_=false,sampling_from_crew_=false;
    bool crew_session_=false;
    unsigned crew_retries_=0,sampling_retry_=0;
    double next_=0,after_=0,deadline_=0,last_activity_=-100,sampled_=-1,stable_at_=0;
    unsigned generation_=0;
    std::string status_="WAITING",stable_key_;
    void invalidate(const std::string& reason);
    void finish_crew_attempt(double now,bool success);
    std::string restore_command() const;
};
}
