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
enum class McduPage { unknown, fpln, dir, fuel, ident, data, init_a, init_b, radnav, menu, sec };
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

// Nonblocking transaction. The caller reads one simulator-thread screen per
// tick and executes the returned MCDU2 page command once. No aircraft writes.
class FmsSampler {
public:
    std::string tick(double now,const McduScreen& screen,bool usable=true);
    void activity(double now); // crew input on either CDU; never swallow it
    void reset();
    void request() {next_=0;}
    // Restore when disabling/stopping, while aircraft handles are still valid.
    // Unsupported original pages fall back to F-PLN.
    std::string cancel();
    const FmsPlan& plan() const {return plan_;}
    const std::string& status() const {return status_;}
    bool busy() const {return stage_!=Stage::idle;}
    bool wants_screen(double now) const {return busy()||(now>=next_&&now-last_activity_>=crew_idle_seconds);}
    double age(double now) const {return sampled_>=0?now-sampled_:-1;}
    unsigned generation() const {return generation_;}
    McduPage saved_page() const {return saved_;}
private:
    static constexpr double crew_idle_seconds=3;
    enum class Stage {idle, detour, fpln, stable, fuel, fuel_stable, verify, verify_stable, restore, restore_wait, ident_wait, init_b_wait};
    Stage stage_=Stage::idle;
    McduPage saved_=McduPage::unknown;
    FmsPlan plan_,pending_;
    bool pending_valid_=false;
    double next_=0,after_=0,deadline_=0,last_activity_=-100,sampled_=-1,stable_at_=0;
    unsigned generation_=0;
    std::string status_="WAITING",stable_key_;
    void invalidate(const std::string& reason);
    std::string restore_command() const;
};
}
