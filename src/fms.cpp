#include "fms.hpp"
#include <algorithm>
#include <cctype>
#include <regex>

namespace oans {
void FmsAutoCapture::update_phase(double now,int phase) {
    phase_=phase>=1&&phase<=10?phase:0;
    next_phase_=now+(phase_>=8?60:phase_>=5?30:5);
}
bool FmsAutoCapture::request_due(double now,bool usable,bool busy) {
    if(!capture_allowed()||!usable||busy||now<next_attempt_)return false;
    next_attempt_=now+30;
    return true;
}
void FmsAutoCapture::observe(const FmsPlan& plan,bool crew_triggered) {
    if(complete_)return;
    // Both airports must be present: two identical partial/empty pages must
    // not stop retries while a datalink plan is still being populated.
    if(!plan.observed||plan.origin.airport.empty()||plan.destination.airport.empty()) {
        confirmations_=0;origin_.clear();destination_.clear();return;
    }
    if(origin_==plan.origin.airport&&destination_==plan.destination.airport)++confirmations_;
    else {origin_=plan.origin.airport;destination_=plan.destination.airport;confirmations_=1;}
    complete_=crew_triggered||confirmations_>=2;
}
bool airport_outside_active_plan(const FmsPlan& plan,const std::string& airport) {
    return plan.observed&&!airport.empty()&&
        airport!=plan.origin.airport&&airport!=plan.destination.airport&&airport!=plan.alternate;
}
namespace {
std::string trim(std::string s) {
    auto a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");
    return a==std::string::npos?"":s.substr(a,b-a+1);
}
bool starts(const std::string& a,const std::string& b){return a.rfind(b,0)==0;}
std::string first_token(const std::string& text){return text.substr(0,text.find(' '));}
std::string fuel_key(const McduScreen& s) {
    return first_token(s.line("cont1","gw"))+"|"+first_token(s.line("cont2","gw"))+
        (s.contains("TMPY")||s.contains("TEMPY")?"|TEMPY":"");
}
FmsEndpoint endpoint(std::string text) {
    static const std::regex re("^([A-Z]{4})([0-9]{1,2}[LCR]?)?( +.*)?$");
    std::smatch m;text=trim(text);
    if(!std::regex_match(text,m,re)||m[1]=="PPOS"||m[1]=="NONE")return {};
    FmsEndpoint e;e.airport=m[1];e.runway=m[2];
    if(!e.runway.empty()) {
        const auto n=std::stoi(e.runway);
        if(n<1||n>36)e.runway.clear();
        else if(e.runway.size()==1||(e.runway.size()==2&&std::isalpha(static_cast<unsigned char>(e.runway.back()))))e.runway="0"+e.runway;
    }
    return e;
}
}
std::string McduScreen::line(const std::string& stem,const std::string& colours) const {
    std::string out;
    for(char c:colours) {
        auto it=fields.find(stem+c);if(it==fields.end())continue;
        const auto& s=it->second;if(s.size()>out.size())out.resize(s.size(),' ');
        for(size_t i=0;i<s.size();++i)if(s[i]!=' '&&s[i]!='\0') {
            // Conflicting coloured glyphs are a changing/ambiguous screen.
            if(out[i]!=' '&&out[i]!=s[i])out[i]='?';else out[i]=s[i];
        }
    }
    return trim(out);
}
bool McduScreen::contains(const std::string& text) const {
    for(const auto& f:fields)if(f.second.find(text)!=std::string::npos)return true;
    return false;
}
bool McduScreen::scratchpad_empty() const{return line("sp").empty();}
std::string McduScreen::plan_key() const {
    std::string s=line("title")+"|"+line("stitle")+"|"+line("label6");
    // Ignore changing predictions to the right of the airport/runway token.
    for(const char* stem:{"cont1","cont6"}) {auto v=line(stem,"gw");s+="|"+v.substr(0,v.find(' '));}
    if(contains("TMPY")||contains("TEMPY"))s+="|TEMPY";
    return s;
}
McduPage mcdu_page(const McduScreen& s) {
    const auto t=s.line("title","wgbamy"),st=s.line("stitle");
    if(s.contains("ACTIVE NAV DATA BASE")&&s.contains("SECOND NAV DATA BASE"))return McduPage::ident;
    if(t=="DIR TO")return McduPage::dir;
    if(t=="FUEL PRED")return McduPage::fuel;
    // DATA INDEX is the DATA main page; subtitle text does not change its identity.
    if(t=="DATA INDEX")return McduPage::data;
    if(t=="RADIO NAV"||t=="RAD NAV")return McduPage::radnav;
    if(t=="MCDU MENU")return McduPage::menu;
    if(t=="SEC INDEX"||t=="SEC F-PLN INDEX")return McduPage::sec;
    if(t=="PERF"||t=="TAKE OFF"||t=="CLB"||t=="CRZ"||t=="DES"||t=="APPR"||t=="GO AROUND")return McduPage::perf;
    if(t=="PROG"||(t.size()>5&&t.substr(t.size()-5)==" PROG"))return McduPage::prog;
    if(t=="ATC"||t=="ATC COMM"||t=="ATC MENU"||t=="ATSU DATALINK")return McduPage::atc;
    if(t=="INIT") {
        if(s.contains("FROM/TO")||s.contains("CO RTE"))return McduPage::init_a;
        if(s.contains("ZFW/ZFWCG")&&s.contains("BLOCK"))return McduPage::init_b;
    }
    // Secondary plans and temporary insertions must never masquerade as active.
    if(s.contains("SEC F-PLN")||s.contains("SECONDARY"))return McduPage::unknown;
    const auto footer=s.line("label6");
    if(first_token(st)=="FROM"||(starts(footer,"DEST")&&footer.find("DIST")!=std::string::npos))return McduPage::fpln;
    if((t.empty()||t=="F-PLN")&&starts(s.line("cont1","gw"),"PPOS")&&
       (s.contains("DISCONT")||s.contains("END OF F-PLN")))return McduPage::fpln;
    return McduPage::unknown;
}
const char* mcdu_page_name(McduPage p) {
    switch(p) {
        case McduPage::fpln:return "F-PLN";case McduPage::dir:return "DIR TO";
        case McduPage::fuel:return "FUEL PRED";case McduPage::ident:return "A/C STATUS";
        case McduPage::data:return "DATA";case McduPage::init_a:return "INIT A";
        case McduPage::init_b:return "INIT B";case McduPage::radnav:return "RAD NAV";
        case McduPage::menu:return "MCDU MENU";case McduPage::sec:return "SEC INDEX";
        case McduPage::perf:return "PERF";case McduPage::prog:return "PROG";
        case McduPage::atc:return "ATC COMM";case McduPage::airport:return "AIR PORT";
        default:return "UNKNOWN";
    }
}
FmsPlan parse_fpln(const McduScreen& s) {
    FmsPlan p;
    if(mcdu_page(s)!=McduPage::fpln){p.reason="NOT F-PLN";return p;}
    if(s.contains("TMPY")||s.contains("TEMPY")){p.reason="TEMPORARY PLAN";return p;}
    p.observed=true;
    // Without FROM, the top line could be a fix or PPOS. Never infer origin.
    if(first_token(s.line("stitle"))=="FROM")p.origin=endpoint(s.line("cont1","gw"));
    if(starts(s.line("label6"),"DEST"))p.destination=endpoint(s.line("cont6","gw"));
    p.reason=p.origin.airport.empty()&&p.destination.airport.empty()?"NO FLIGHT PLAN":
        p.origin.runway.empty()||p.destination.runway.empty()?"AIRPORTS / RUNWAY UNAVAILABLE":"CONNECTED";
    return p;
}
bool parse_fuel_pred(const McduScreen& s,FmsPlan& p) {
    if(!p.observed||p.destination.airport.empty()||mcdu_page(s)!=McduPage::fuel||
       s.contains("TMPY")||s.contains("TEMPY"))return false;
    if(first_token(s.line("cont1","gw"))!=p.destination.airport)return false;
    const auto token=first_token(s.line("cont2","gw"));
    if(token.empty()||token=="----") {p.alternate.clear();return true;}
    const auto e=endpoint(token);
    if(e.airport.empty()||token!=e.airport)return false;
    p.alternate=e.airport;return true;
}
void FmsSampler::invalidate(const std::string& why) {
    plan_={};plan_.reason=why;sampled_=-1;++generation_;
}
void FmsSampler::reset(bool initial_sample){*this=FmsSampler{initial_sample};}
void FmsSampler::activity(double now,bool request_sample) {
    last_activity_=now;next_=now+crew_idle_seconds;
    const bool interrupted_crew=busy()&&sampling_from_crew_;
    if(request_sample||interrupted_crew) {
        requested_=true;requested_by_crew_=true;crew_session_=true;crew_retries_=0;
    }
    if(busy()) {stage_=Stage::idle;pending_valid_=false;invalidate("CREW INTERRUPTED");status_="CREW INTERRUPTED";}
}
void FmsSampler::finish_crew_attempt(double now,bool success) {
    if(!sampling_from_crew_||!crew_session_)return;
    if(success||crew_retries_>=max_crew_retries){crew_session_=false;return;}
    ++crew_retries_;requested_=true;requested_by_crew_=true;next_=now+crew_idle_seconds;
}
void FmsSampler::stop_automatic_capture() {
    if(!requested_by_crew_)requested_=false;
    if(!busy()||sampling_from_crew_||stage_==Stage::restore||stage_==Stage::restore_wait||
       stage_==Stage::ident_wait||stage_==Stage::init_b_wait)return;
    pending_valid_=false;invalidate("ECAM PHASE BLOCKED");status_="ECAM PHASE BLOCKED";stage_=Stage::restore;
}
std::string FmsSampler::restore_command() const {
    switch(saved_) {
        case McduPage::fpln:return "Fpln";case McduPage::dir:return "DirTo";
        case McduPage::fuel:return "FuelPred";case McduPage::ident:case McduPage::data:return "Data";
        case McduPage::init_a:case McduPage::init_b:return "Init";
        case McduPage::radnav:return "RadNav";case McduPage::menu:return "Menu";
        case McduPage::sec:return "SecFpln";case McduPage::perf:return "Perf";
        case McduPage::prog:return "Prog";case McduPage::atc:return "ATC";
        case McduPage::airport:return "Airport";default:return "";
    }
}
std::string FmsSampler::cancel() {
    auto command=busy()?restore_command():"";
    stage_=Stage::idle;pending_valid_=false;invalidate("DISCONNECTED");status_="DISCONNECTED";
    return command;
}
std::string FmsSampler::tick(double now,const McduScreen& s,bool usable,McduPage restore_page) {
    if(!usable){if(busy()){auto command=cancel();finish_crew_attempt(now,false);return command;}if(plan_.observed)invalidate("MCDU2 UNAVAILABLE");status_="MCDU2 UNAVAILABLE";return "";}
    if(stage_==Stage::idle) {
        if(!requested_||now<next_||now-last_activity_<crew_idle_seconds)return "";
        requested_=false;
        sampling_from_crew_=requested_by_crew_;requested_by_crew_=false;
        sampling_retry_=sampling_from_crew_?crew_retries_:0;
        if(!s.scratchpad_empty()||s.contains("TMPY")||s.contains("TEMPY")){status_="MCDU BUSY";finish_crew_attempt(now,false);return "";}
        // The port supplies the tracked parent even when screen text belongs
        // to an unrecognized subpage. Snapshot once, before any internal keys.
        saved_=restore_page==McduPage::unknown?mcdu_page(s):restore_page;
        // No verified restore path: sample anyway and finish on F-PLN.
        if(saved_==McduPage::unknown)saved_=McduPage::fpln;
        pending_valid_=false;stable_key_.clear();stage_=Stage::detour;after_=now+.2;deadline_=now+3;
        status_="SAMPLING MCDU2";
        return saved_==McduPage::dir?"Data":"DirTo";
    }
    if(now<after_)return "";
    if(stage_==Stage::restore) {
        stage_=Stage::restore_wait;after_=now+.2;deadline_=now+3;return restore_command();
    }
    if(now>deadline_) {
        if(stage_==Stage::restore_wait||stage_==Stage::ident_wait||stage_==Stage::init_b_wait) {
            stage_=Stage::idle;pending_valid_=false;invalidate("RESTORE FAILED");status_="RESTORE FAILED";
            finish_crew_attempt(now,false);
        } else {pending_valid_=false;invalidate("PAGE TIMEOUT");stage_=Stage::restore;status_="PAGE TIMEOUT";}
        return "";
    }
    const auto page=mcdu_page(s);
    if(stage_==Stage::detour) {
        if(page!=(saved_==McduPage::dir?McduPage::data:McduPage::dir))return "";
        stage_=Stage::fpln;after_=now+.2;deadline_=now+3;return "Fpln";
    }
    if(stage_==Stage::fpln||stage_==Stage::stable) {
        if(page!=McduPage::fpln)return "";
        const auto key=s.plan_key();
        if(stage_==Stage::fpln||key!=stable_key_){stage_=Stage::stable;stable_key_=key;stable_at_=now;return "";}
        if(now-stable_at_<.15)return "";
        pending_=parse_fpln(s);pending_valid_=pending_.observed;
        if(pending_valid_&&!pending_.destination.airport.empty()) {
            stage_=Stage::fuel;after_=now+.2;deadline_=now+3;return "FuelPred";
        }
        if(!pending_valid_){invalidate(pending_.reason);status_=pending_.reason;}
        stage_=Stage::restore;return "";
    }
    if(stage_==Stage::fuel||stage_==Stage::fuel_stable) {
        if(page!=McduPage::fuel)return "";
        const auto key=fuel_key(s);
        if(stage_==Stage::fuel||key!=stable_key_){stage_=Stage::fuel_stable;stable_key_=key;stable_at_=now;return "";}
        if(now-stable_at_<.15)return "";
        if(!parse_fuel_pred(s,pending_)) {
            pending_valid_=false;invalidate("FUEL PAGE MISMATCH");status_="FUEL PAGE MISMATCH";stage_=Stage::restore;return "";
        }
        // Verify the endpoints again after crossing pages, before publishing ALTN.
        stage_=Stage::verify;after_=now+.2;deadline_=now+3;return "Fpln";
    }
    if(stage_==Stage::verify||stage_==Stage::verify_stable) {
        if(page!=McduPage::fpln)return "";
        const auto key=s.plan_key();
        if(stage_==Stage::verify||key!=stable_key_){stage_=Stage::verify_stable;stable_key_=key;stable_at_=now;return "";}
        if(now-stable_at_<.15)return "";
        const auto current=parse_fpln(s);
        if(!current.observed||current.origin.airport!=pending_.origin.airport||current.origin.runway!=pending_.origin.runway||
           current.destination.airport!=pending_.destination.airport||current.destination.runway!=pending_.destination.runway) {
            pending_valid_=false;invalidate("PLAN CHANGED");status_="PLAN CHANGED";
        }
        stage_=Stage::restore;return "";
    }
    if(stage_==Stage::restore_wait) {
        if(saved_==McduPage::ident) {
            // The only LSK used is a verified navigation item, never a plan edit.
            if(page==McduPage::data&&s.line("cont4")=="<A/C STATUS") {
                stage_=Stage::ident_wait;after_=now+.2;deadline_=now+3;return "LSK4L";
            }
            return "";
        }
        if(saved_==McduPage::init_b) {
            if(page==McduPage::init_a){stage_=Stage::init_b_wait;after_=now+.2;deadline_=now+3;return "SlewRight";}
            if(page!=McduPage::init_b)return "";
        } else if(saved_==McduPage::airport? page!=McduPage::fpln : page!=saved_)return "";
    }
    if((stage_==Stage::ident_wait&&page!=McduPage::ident)||(stage_==Stage::init_b_wait&&page!=McduPage::init_b))return "";
    if(pending_valid_) {plan_=pending_;sampled_=now;++generation_;status_=plan_.reason;}
    // Publish only after the saved page has been restored and verified.
    pending_valid_=false;stage_=Stage::idle;
    finish_crew_attempt(now,plan_.observed&&!plan_.origin.airport.empty()&&!plan_.destination.airport.empty());
    return "";
}
}
