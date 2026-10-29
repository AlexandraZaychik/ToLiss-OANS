#include "fms_xplm.hpp"
#include "detail_log.hpp"
#include <XPLMProcessing.h>
#include <algorithm>
#include <cstring>

namespace oans {
McduScreen McduFmsPort::read() const {
    OANS_TRACE_SCOPE();
    McduScreen s;
    for(const auto& r:text_) {
        OANS_TRACE("MCDU text read field="+r.first);
        char buffer[128]={};const auto count=XPLMGetDatab(r.second,buffer,0,127);
        if(count>0) s.fields[r.first]=std::string(buffer,std::find(buffer,buffer+std::min(count,127),'\0'));
    }
    return s;
}
void McduFmsPort::send(const std::string& suffix) {
    OANS_TRACE_SCOPE();
    OANS_TRACE("MCDU command="+suffix);
    auto it=pages_.find(suffix);if(it==pages_.end()||!it->second)return;
    issuing_=true;XPLMCommandOnce(it->second);issuing_=false;
}
void McduFmsPort::finish_detection(const std::string& result) {
    if(!detection_source_.empty()&&log_event)log_event(detection_source_+" "+result);
    detection_source_.clear();
}
int McduFmsPort::activity(XPLMCommandRef command,XPLMCommandPhase phase,void* ptr) {
    OANS_TRACE_SCOPE();
    auto& p=*static_cast<McduFmsPort*>(ptr);
    if(!p.issuing_&&(phase==xplm_CommandBegin||phase==xplm_CommandContinue)) {
        const auto it=p.observed_.find(command);
        if(it!=p.observed_.end()&&it->second.unit==2)p.mcdu2_operated_=true;
        bool request_sample=false;
        if(phase==xplm_CommandBegin&&it!=p.observed_.end()) {
            const auto& key=it->second;
            const auto& parent=p.major_pages_[key.unit-1];
            request_sample=(parent=="INIT"&&(key.key=="LSK1R"||key.key=="LSK2L"))||
                           (parent=="F-PLN"&&key.key=="LSK6R");
            if(request_sample)p.trigger_source_="trigger detect "+parent+" "+key.key;
        }
        if(p.sampler.busy()) {
            p.refresh_phase(XPLMGetElapsedTime());
            p.finish_detection("fail (crew interrupted)");
        }
        p.sampler.activity(XPLMGetElapsedTime(),request_sample);
        if(phase==xplm_CommandBegin&&it!=p.observed_.end()&&!it->second.major.empty()) {
            p.major_pages_[it->second.unit-1]=it->second.major;
            OANS_TRACE("MCDU"+std::to_string(it->second.unit)+" major_page="+it->second.major);
        }
    }
    return 1;
}
bool McduFmsPort::bind() {
    OANS_TRACE_SCOPE();
    bind_activity();
    if(ready())return true;
    auto bind_text=[&](std::string name) {
        OANS_TRACE("MCDU bind text="+name);
        auto r=XPLMFindDataRef(("AirbusFBW/MCDU2"+name).c_str());
        OANS_TRACE("MCDU bind text found="+std::to_string(r!=nullptr));
        if(r&&(XPLMGetDataRefTypes(r)&xplmType_Data))text_[name]=r;
    };
    for(const char* stem:{"title","stitle"})for(char c:std::string("wgbamsy"))bind_text(std::string(stem)+c);
    for(const char* stem:{"label","cont","scont"})for(int i=1;i<=6;++i)
        for(char c:std::string("wgbamsy"))bind_text(std::string(stem)+std::to_string(i)+c);
    for(char c:std::string("wa"))bind_text(std::string("sp")+c);
    if(!text_.count("cont1g")||!text_.count("cont2g")||!text_.count("cont6w")||!text_.count("titlew")){text_.clear();return false;}
    const std::vector<std::string> page_keys={"Init","Fpln","Perf","RadNav","DirTo","Prog","Data","FuelPred","SecFpln","ATC","Menu","Airport","SlewDown","SlewUp","SlewLeft","SlewRight"};
    for(const auto& key:page_keys)pages_[key]=XPLMFindCommand(("AirbusFBW/MCDU2"+key).c_str());
    pages_["LSK4L"]=XPLMFindCommand("AirbusFBW/MCDU2LSK4L");
    for(const char* key:{"DirTo","Fpln","FuelPred","Data","LSK4L","SlewRight"})if(!pages_[key]){pages_.clear();text_.clear();return false;}
    return true;
}
void McduFmsPort::bind_activity() {
    const std::map<std::string,std::string> major_keys={
        {"DirTo","DIR"},{"Prog","PROG"},{"Perf","PERF"},{"Init","INIT"},{"Data","DATA"},
        {"Fpln","F-PLN"},{"RadNav","RAD NAV"},{"FuelPred","FUEL PRED"},
        {"SecFpln","SEC F-PLN"},{"ATC","ATC COMM"},{"Menu","MCDU MENU"},{"Airport","AIR PORT"}};
    // Retry missing commands without duplicating registered handlers. Independent
    // of the MCDU2 text provider and automatic sampling setting.
    // Observe both keyboards: MCDU1 route edits must also defer background reads.
    // Never intercept/suppress input and never observe our own dispatch as crew input.
    for(int unit:{1,2}) {
        std::vector<std::string> keys;
        for(const auto& key:major_keys)keys.push_back(key.first);
        for(const char* key:{"SlewDown","SlewUp","SlewLeft","SlewRight"})keys.emplace_back(key);
        for(int i=1;i<=6;++i)for(char c:std::string("LR"))keys.push_back("LSK"+std::to_string(i)+c);
        for(char c:std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"))keys.push_back(std::string("Key")+c);
        for(const char* k:{"KeySlash","KeySpace","KeyOverfly","KeyClear","KeyDecimal","KeyPM","KeyBright","KeyDim"})keys.emplace_back(k);
        for(const auto& key:keys)if(auto cmd=XPLMFindCommand(("AirbusFBW/MCDU"+std::to_string(unit)+key).c_str())) {
            if(observed_.count(cmd))continue;
            auto major=major_keys.find(key);
            observed_.emplace(cmd,ObservedKey{unit,major==major_keys.end()?"":major->second,key});
            XPLMRegisterCommandHandler(cmd,activity,1,this);
        }
    }
}
void McduFmsPort::refresh_phase(double now) {
    // Resolve again: a missing provider may appear after initialization.
    // Refresh during transactions too, so publication/interruption uses the current phase.
    auto ref=XPLMFindDataRef("AirbusFBW/ECAMFlightPhase");
    const int phase=ref&&(XPLMGetDataRefTypes(ref)&xplmType_Int)?XPLMGetDatai(ref):0;
    automatic.update_phase(now,phase);
    sampler.set_flight_phase(automatic.phase());
}
void McduFmsPort::tick(double now,bool usable) {
    OANS_TRACE_SCOPE();
    if(!ready())return;
    if(automatic.phase_due(now)||sampler.wants_screen(now))refresh_phase(now);
    if(!automatic.capture_allowed())sampler.stop_automatic_capture();
    if(automatic.request_due(now,usable,sampler.busy()))sampler.request();
    McduScreen screen;
    if(usable&&sampler.wants_screen(now)) {
        if(now<next_read_)return;
        next_read_=now+.1;screen=read();
    }
    OANS_TRACE("MCDU sampler usable="+std::to_string(usable)+" busy="+std::to_string(sampler.busy())+" generation="+std::to_string(sampler.generation()));
    static const std::map<std::string,McduPage> parents={
        {"DATA",McduPage::data},{"DIR",McduPage::dir},{"PROG",McduPage::prog},
        {"PERF",McduPage::perf},{"INIT",McduPage::init_a},{"F-PLN",McduPage::fpln},
        {"RAD NAV",McduPage::radnav},{"FUEL PRED",McduPage::fuel},{"SEC F-PLN",McduPage::sec},
        {"ATC COMM",McduPage::atc},{"MCDU MENU",McduPage::menu},{"AIR PORT",McduPage::airport}};
    const bool was_busy=sampler.busy();
    const bool starting=usable&&!was_busy&&sampler.wants_screen(now);
    // An untouched MCDU2 starts on A/C STATUS. Restore it through DATA INDEX
    // and its verified LSK4L item; our own page commands never mark it operated.
    const auto restore_page=mcdu2_operated_?parents.at(major_page(2)):McduPage::ident;
    const auto command=sampler.tick(now,screen,usable,restore_page);
    if(starting) {
        if(sampler.crew_triggered()) {
            detection_source_=trigger_source_;
            if(sampler.crew_retry())detection_source_+=" retry "+std::to_string(sampler.crew_retry());
        } else detection_source_="auto detect try "+std::to_string(++auto_attempts_);
        if(log_event)log_event(detection_source_);
        if(!sampler.busy()) {
            const bool draft=screen.contains("TMPY")||screen.contains("TEMPY");
            const bool input=!screen.scratchpad_empty();
            finish_detection(std::string("fail (")+(draft&&input?"have draft / have input":draft?"have draft":"have input")+")");
        }
    }
    if(!command.empty())send(command);
    if(generation_seen_!=sampler.generation()) {
        generation_seen_=sampler.generation();
        automatic.observe(sampler.complete_pair()?sampler.sampled_plan():FmsPlan{});
    }
    if(!detection_source_.empty()&&!sampler.busy()) {
        const bool success=sampler.complete_pair();
        finish_detection(success?"success":"fail ("+sampler.status()+")");
    }
}
void McduFmsPort::clear(bool restore) {
    OANS_TRACE_SCOPE();
    finish_detection("fail (disconnected)");
    if(restore&&ready()) {
        const auto page=sampler.saved_page();const auto command=sampler.cancel();
        if(!command.empty()) {
            send(command);
            // Some providers update text synchronously; only use the verified
            // navigation LSK when the DATA page is already confirmed.
            auto s=read();
            if(page==McduPage::ident&&mcdu_page(s)==McduPage::data&&s.line("cont4")=="<A/C STATUS")send("LSK4L");
            if(page==McduPage::init_b&&mcdu_page(s)==McduPage::init_a)send("SlewRight");
        }
    }
    for(const auto& c:observed_)XPLMUnregisterCommandHandler(c.first,activity,1,this);
    observed_.clear();pages_.clear();text_.clear();next_read_=0;sampler.reset(false);
    automatic=FmsAutoCapture{};generation_seen_=0;auto_attempts_=0;trigger_source_.clear();
}
}
