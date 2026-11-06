#include "fms_xplm.hpp"
#include <XPLMProcessing.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>
namespace oans::zibo {
namespace {
std::string text(XPLMDataRef ref) {
    char buffer[64]{};
    const int count=XPLMGetDatab(ref,buffer,0,63);
    if(count<=0)return {};
    std::string value(buffer,std::find(buffer,buffer+std::min(count,63),'\0'));
    const auto first=value.find_first_not_of(" \t\r\n"),last=value.find_last_not_of(" \t\r\n");
    value=first==std::string::npos?std::string{}:value.substr(first,last-first+1);
    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});
    return value;
}
bool airport(const std::string& value) {
    return value.size()==4&&value!="0000"&&std::all_of(value.begin(),value.end(),[](unsigned char c){return (c>='A'&&c<='Z')||(c>='0'&&c<='9');});
}
bool runway(const std::string& value) {
    if(value.size()!=2&&value.size()!=3)return false;
    if(value[0]<'0'||value[0]>'3'||value[1]<'0'||value[1]>'9')return false;
    const int number=(value[0]-'0')*10+value[1]-'0';
    return number>=1&&number<=36&&(value.size()==2||value[2]=='L'||value[2]=='C'||value[2]=='R');
}
}
bool DatarefFmsPort::ready() const {
    return std::all_of(endpoints_.begin(),endpoints_.end(),[](XPLMDataRef r){return r!=nullptr;});
}
bool DatarefFmsPort::bind() {
    const char* fields[]={"ref_icao","ref_runway","dest_icao","dest_runway"};
    for(int i=0;i<4;++i) {
        auto ref=XPLMFindDataRef((std::string("laminar/B738/fms/")+fields[i]).c_str());
        endpoints_[i]=ref&&(XPLMGetDataRefTypes(ref)&xplmType_Data)?ref:nullptr;
    }
    auto ref=XPLMFindDataRef("laminar/B738/FMS/flight_phase");
    phase_=ref&&(XPLMGetDataRefTypes(ref)&(xplmType_Int|xplmType_Float|xplmType_Double))?ref:nullptr;
    bind_activity();return ready();
}
void DatarefFmsPort::bind_activity() {
    // The 69 key suffixes are the commands published by Zibo's B738.a_fms.lua.
    std::vector<std::string> keys;
    for(char c:std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"))keys.emplace_back(1,c);
    for(int n=1;n<=6;++n)for(char side:std::string("LR"))keys.push_back(std::to_string(n)+side);
    for(const char* key:{"SP","clb","clr","crz","del","dep_app","des","exec","fix","hold","init_ref","legs","menu","minus","n1_lim","next_page","period","prev_page","prog","rte","slash"})keys.emplace_back(key);
    for(int unit:{1,2})for(const auto& key:keys) {
        const auto name="laminar/B738/button/fmc"+std::to_string(unit)+"_"+key;
        if(auto cmd=XPLMFindCommand(name.c_str())) {
            if(observed_.count(cmd))continue;
            observed_.emplace(cmd,name);
            XPLMRegisterCommandHandler(cmd,activity,1,this);
        }
    }
}
int DatarefFmsPort::activity(XPLMCommandRef cmd,XPLMCommandPhase phase,void* ptr) {
    auto& p=*static_cast<DatarefFmsPort*>(ptr);
    if(phase==xplm_CommandBegin||phase==xplm_CommandContinue) {
        p.pending_=true;
        p.read_at_=XPLMGetElapsedTime()+3;
        auto found=p.observed_.find(cmd);p.trigger_=found==p.observed_.end()?"FMC KEY":found->second;
    }
    return 1; // Observe only; aircraft command handlers still receive the key.
}
int DatarefFmsPort::phase() const {
    if(!phase_)return -1;
    const auto type=XPLMGetDataRefTypes(phase_);
    const double value=type&xplmType_Double?XPLMGetDatad(phase_):type&xplmType_Float?XPLMGetDataf(phase_):XPLMGetDatai(phase_);
    return std::isfinite(value)&&value>=0&&value<=8&&std::floor(value)==value?static_cast<int>(value):-1;
}
void DatarefFmsPort::sample(double now,const std::string& trigger) {
    const auto origin=text(endpoints_[0]),departure=text(endpoints_[1]);
    const auto destination=text(endpoints_[2]),arrival=text(endpoints_[3]);
    FmsPlan next;
    if(airport(origin))next.origin={origin,runway(departure)?departure:""};
    if(airport(destination))next.destination={destination,runway(arrival)?arrival:""};
    next.observed=!next.origin.airport.empty()||!next.destination.airport.empty();
    next.reason=next.observed?"ZIBO DATAREF; ALTERNATE UNAVAILABLE":"ZIBO FLIGHT PLAN EMPTY";
    plan_=std::move(next);status_=plan_.reason;last_read_=now;++generation_;
    if(log_event)log_event("ZIBO FMS trigger="+trigger+" phase="+std::to_string(last_phase_)+
        " origin="+plan_.origin.airport+" departure_runway="+plan_.origin.runway+
        " destination="+plan_.destination.airport+" arrival_runway="+plan_.destination.runway+" alternate=UNAVAILABLE");
}
void DatarefFmsPort::tick(double now,bool usable) {
    if(!usable||!ready()) {pending_=false;read_at_=0;return;}
    if(now>=next_phase_) {
        last_phase_=phase();
        next_phase_=now+3;
    }
    if(pending_&&now>=read_at_) {pending_=false;sample(now,trigger_);}
}
void DatarefFmsPort::clear(bool) {
    for(const auto& c:observed_)XPLMUnregisterCommandHandler(c.first,activity,1,this);
    observed_.clear();endpoints_={};phase_=nullptr;plan_={};status_="NOT SAMPLED";
    pending_=false;trigger_.clear();read_at_=0;last_read_=-1;last_phase_=-1;next_phase_=0;generation_=0;
}
void DatarefFmsPort::diagnostics(std::ostream& out,double now,bool enabled) const {
    out<<"FMS source=ZIBO_DATAREF enabled="<<enabled<<" bound="<<ready()<<" phase="<<last_phase_
       <<" auto_sampling=0 key_delay_s=3 key_observers="<<observed_.size()<<" pending="<<pending_
       <<" observed="<<plan_.observed<<" age="<<(last_read_<0?-1:now-last_read_)<<" generation="<<generation_
       <<" status="<<status_<<"\n"
       <<"FMS_PLAN origin="<<plan_.origin.airport<<" departure_runway="<<plan_.origin.runway
       <<" destination="<<plan_.destination.airport<<" arrival_runway="<<plan_.destination.runway
       <<" alternate=UNAVAILABLE\n";
}
}
