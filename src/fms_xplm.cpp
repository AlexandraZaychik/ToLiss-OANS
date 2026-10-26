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
int McduFmsPort::activity(XPLMCommandRef,XPLMCommandPhase phase,void* ptr) {
    OANS_TRACE_SCOPE();
    auto& p=*static_cast<McduFmsPort*>(ptr);
    if(!p.issuing_&&(phase==xplm_CommandBegin||phase==xplm_CommandContinue))p.sampler.activity(XPLMGetElapsedTime());
    return 1;
}
bool McduFmsPort::bind() {
    OANS_TRACE_SCOPE();
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
    // Observe both keyboards: MCDU1 route edits must also defer background reads.
    // Never intercept/suppress input and never observe our own dispatch as crew input.
    for(int unit:{1,2}) {
        std::vector<std::string> keys=page_keys;
        for(int i=1;i<=6;++i)for(char c:std::string("LR"))keys.push_back("LSK"+std::to_string(i)+c);
        for(char c:std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"))keys.push_back(std::string("Key")+c);
        for(const char* k:{"KeySlash","KeySpace","KeyOverfly","KeyClear","KeyDecimal","KeyPM","KeyBright","KeyDim"})keys.emplace_back(k);
        for(const auto& key:keys)if(auto cmd=XPLMFindCommand(("AirbusFBW/MCDU"+std::to_string(unit)+key).c_str())) {
            XPLMRegisterCommandHandler(cmd,activity,1,this);observed_.push_back(cmd);
        }
    }
    return true;
}
void McduFmsPort::tick(double now,bool usable) {
    OANS_TRACE_SCOPE();
    if(!ready())return;
    McduScreen screen;
    if(usable&&sampler.wants_screen(now)) {
        if(now<next_read_)return;
        next_read_=now+.1;screen=read();
    }
    OANS_TRACE("MCDU sampler usable="+std::to_string(usable)+" busy="+std::to_string(sampler.busy())+" generation="+std::to_string(sampler.generation()));
    const auto command=sampler.tick(now,screen,usable);if(!command.empty())send(command);
}
void McduFmsPort::clear(bool restore) {
    OANS_TRACE_SCOPE();
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
    for(auto c:observed_)XPLMUnregisterCommandHandler(c,activity,1,this);
    observed_.clear();pages_.clear();text_.clear();next_read_=0;sampler.reset();
}
}
