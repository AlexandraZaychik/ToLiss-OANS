#include "fms.hpp"
#include <iostream>
#include <stdexcept>
using namespace oans;
int checks=0;
void check(bool v,const char* text){++checks;if(!v)throw std::runtime_error(text);}
McduScreen plan(std::string from="ZUCK02L",std::string to="ZUUU02L") {
    McduScreen s;s.fields={{"stitlew","  FROM "},{"label6w","DEST    TIME  DIST  EFOB"},
        {"cont1g",from},{"cont6w",to+" 0038 213 11.4"},{"titles","                      23"}};return s;
}
McduScreen page(const std::string& title){McduScreen s;s.fields["titlew"]=title;return s;}
McduScreen data(){auto s=page("DATA INDEX");s.fields["stitlew"]="1/2";s.fields["titles"]="                      23";s.fields["cont4w"]="<A/C STATUS";return s;}
McduScreen ident(){auto s=page("A320-251N");s.fields["label2w"]="ACTIVE NAV DATA BASE";s.fields["label3w"]="SECOND NAV DATA BASE";return s;}
McduScreen fuel(std::string dest="ZUUU",std::string alternate="ZALT") {
    auto s=page("FUEL PRED");s.fields["cont1g"]=dest+"     0000        0.0";
    s.fields["cont2g"]=alternate+"     0000        0.0";return s;
}
void sample_pages(FmsSampler& a,double t,const McduScreen& flightplan,const McduScreen& prediction) {
    a.tick(t+.5,flightplan);
    auto command=a.tick(t+.7,flightplan);
    if(parse_fpln(flightplan).destination.airport.empty())return;
    check(command=="FuelPred","sample alternate on FUEL PRED after stable F-PLN");
    a.tick(t+.95,prediction);
    check(a.tick(t+1.15,prediction)=="Fpln","verify F-PLN after stable fuel prediction");
    a.tick(t+1.4,flightplan);a.tick(t+1.6,flightplan);
}
void cycle(FmsSampler& a,double t,const McduScreen& initial,const McduScreen& flightplan,const McduScreen& prediction=fuel()) {
    const auto generation=a.generation();
    check(a.tick(t,initial)==(mcdu_page(initial)==McduPage::dir?"Data":"DirTo"),"detour first, even from F-PLN");
    check(a.tick(t+.25,mcdu_page(initial)==McduPage::dir?data():page("DIR TO"))=="Fpln","force reenter F-PLN from another page");
    sample_pages(a,t,flightplan,prediction);
    check(a.generation()==generation,"do not publish before restoration");
    auto command=a.tick(t+1.8,flightplan);
    if(mcdu_page(initial)==McduPage::ident) {
        check(command=="Data","IDENT restore starts with DATA");
        check(a.tick(t+2.05,data())=="LSK4L","only verified A/C STATUS navigation LSK");
        a.tick(t+2.3,initial);
    } else if(mcdu_page(initial)==McduPage::init_b) {
        check(command=="Init","INIT B restore starts with INIT");
        McduScreen init=page("INIT");init.fields["label1w"]="CO RTE";
        check(a.tick(t+2.05,init)=="SlewRight","return to INIT B with page arrow");a.tick(t+2.3,initial);
    } else if(mcdu_page(initial)==McduPage::unknown) {
        check(command=="Fpln","unsupported original page falls back to F-PLN");
        a.tick(t+2.05,flightplan);
    } else a.tick(t+2.05,initial);
    check(!a.busy(),"restoration finishes transaction");
}
void tests() {
    auto membership=parse_fpln(plan());
    check(!airport_outside_active_plan(membership,"ZUCK")&&!airport_outside_active_plan(membership,"ZUUU"),"origin and destination belong to active plan");
    check(airport_outside_active_plan(membership,"LFBO"),"unrelated airport is outside observed plan");
    membership.observed=false;check(!airport_outside_active_plan(membership,"LFBO"),"unknown or stale plan never asserts non-membership");
    check(!airport_outside_active_plan(FmsPlan{},"LFBO"),"empty plan has no false non-membership warning");
    check(airport_outside_active_plan(parse_fpln(plan("PPOS","")),"LFBO"),"confirmed empty active plan contains no selected airport");
    auto p=parse_fpln(plan());check(p.observed&&p.origin.airport=="ZUCK"&&p.origin.runway=="02L"&&p.destination.airport=="ZUUU"&&p.destination.runway=="02L","actual ZUCK-ZUUU layout");
    p=parse_fpln(plan("ZUCK","ZUUU"));check(p.origin.airport=="ZUCK"&&p.origin.runway.empty()&&p.destination.airport=="ZUUU"&&p.destination.runway.empty(),"airports remain valid without runways");
    p=parse_fpln(plan("ZUCK02L","ZUUU"));check(p.origin.runway=="02L"&&p.destination.runway.empty(),"independent missing arrival runway");
    p=parse_fpln(plan("PPOS","----"));check(p.observed&&p.origin.airport.empty()&&p.destination.airport.empty(),"PPOS is never an airport");
    p=parse_fpln(plan("ZUCK37L","ZUUU00"));check(p.origin.airport=="ZUCK"&&p.origin.runway.empty()&&p.destination.runway.empty(),"reject runway 00 and 37");
    p=parse_fpln(plan("KJFK4L","KLAX7"));check(p.origin.runway=="04L"&&p.destination.runway=="07","normalize one digit runway");
    auto s=plan();s.fields["stitlew"]="TO";s.fields["cont1g"]="HANG";p=parse_fpln(s);
    check(p.origin.airport.empty()&&p.destination.airport=="ZUUU","a fix cannot become the origin without FROM");
    s=plan();s.fields["titley"]="TMPY";p=parse_fpln(s);check(!p.observed&&p.destination.airport.empty(),"temporary plan not published as active");
    s=plan();s.fields["cont1w"]="ZBAA16L";p=parse_fpln(s);check(p.origin.airport.empty(),"conflicting text layers are rejected");
    s=page("FUEL PRED");s.fields["cont1g"]="ZUUU 0038 11.4";
    check(!parse_fpln(s).observed,"FUEL PRED cannot masquerade as origin");
    p=parse_fpln(plan("ZBAA","ZSSS"));
    check(parse_fuel_pred(fuel("ZSSS","ZUCK"),p)&&p.alternate=="ZUCK","live ZSSS destination and ZUCK alternate fields");
    check(!airport_outside_active_plan(p,"ZUCK"),"alternate is a member of the active plan");
    check(!parse_fuel_pred(fuel("ZUUU","ZALT"),p)&&p.alternate=="ZUCK","destination mismatch does not replace alternate");
    check(!parse_fuel_pred(plan(),p),"other pages cannot masquerade as fuel prediction");
    s=fuel("ZSSS","ZALT");s.fields["cont2w"]="ZBAA";
    check(!parse_fuel_pred(s,p),"conflicting alternate colour layers are rejected");
    for(const char* token:{"PPOS","NONE","ZUCK02L","0000","ZUC?"})
        check(!parse_fuel_pred(fuel("ZSSS",token),p),"invalid alternate tokens are rejected");
    s=fuel("ZSSS","ZUCK");s.fields["titley"]="TMPY";
    check(!parse_fuel_pred(s,p),"temporary fuel page is rejected");
    s=fuel("ZSSS","----");s.fields.erase("cont2g");s.fields["cont2w"]="----    ----      ----.-";
    check(parse_fuel_pred(s,p)&&p.alternate.empty(),"white dashed row clears old alternate");
    p.alternate="ZUCK";s.fields.erase("cont2w");
    check(parse_fuel_pred(s,p)&&p.alternate.empty(),"blank alternate row clears old alternate");
    check(mcdu_page(data())==McduPage::data,"ignore small title decoration on DATA INDEX");
    auto empty=page("");empty.fields["cont1g"]="PPOS";empty.fields["cont2w"]="F-PLN DISCONTINUITY";
    p=parse_fpln(empty);check(p.observed&&p.reason=="NO FLIGHT PLAN","empty PPOS plan without destination footer");
    FmsSampler a;cycle(a,0,ident(),plan());check(a.plan().destination.runway=="02L"&&a.plan().alternate=="ZALT","IDENT round trip publishes endpoints and alternate");
    cycle(a,40,page("FUEL PRED"),plan("ZUCK","ZUUU"),fuel("ZUUU","----"));check(a.plan().destination.runway.empty()&&a.plan().alternate.empty(),"new sample clears deselected runway and alternate");
    cycle(a,80,page("DIR TO"),empty);check(a.plan().observed&&a.plan().destination.airport.empty()&&a.plan().alternate.empty(),"new empty plan clears old airports and alternate");
    cycle(a,120,plan(),plan());check(a.plan().observed,"sampling from F-PLN still resets through detour");
    a.tick(220,page("UNKNOWN"));check(!a.plan().observed&&a.plan().destination.airport.empty(),"stale cache expires even on unsupported page");
    for(const char* title:{"UNKNOWN","PERF","PROG","ATC"}) {
        a.reset();cycle(a,0,page(title),plan());
        check(a.plan().observed&&a.plan().alternate=="ZALT","unsupported original page publishes verified plan and alternate");
    }
    a.reset();a.tick(0,page("UNKNOWN"));a.tick(4,page("UNKNOWN"));
    check(a.tick(4.1,page("UNKNOWN"))=="Fpln","unsupported original page timeout falls back to F-PLN");
    a.tick(4.4,plan());check(!a.busy()&&!a.plan().observed,"failed sample from unsupported page never publishes");
    a.reset();a.tick(0,page("UNKNOWN"));a.activity(.1);
    check(!a.busy()&&a.tick(.5,page("UNKNOWN")).empty(),"crew interrupts unsupported-page sample without restore command");
    a.reset();s=ident();s.fields["spw"]="ZBAA";check(a.tick(0,s).empty(),"scratchpad input defers sampling");
    a.reset();a.activity(0);check(!a.wants_screen(2.9)&&a.tick(2.9,ident()).empty(),"crew cooldown defers reads and sampling");
    a.activity(2);check(!a.wants_screen(4.9)&&a.tick(4.9,ident()).empty(),"each crew input restarts three second cooldown");
    check(a.wants_screen(5)&&a.tick(5,ident())=="DirTo","resume exactly three seconds after last input");
    a.activity(5.1);check(!a.busy()&&!a.plan().observed&&a.tick(5.4,page("DIR TO")).empty(),"crew interruption never fights back with restore keys");
    a.reset();a.tick(0,ident());a.tick(4,ident());check(a.tick(4.1,ident())=="Data","page timeout attempts restoration");
    a.tick(4.4,data());a.tick(4.7,ident());check(!a.busy()&&!a.plan().observed,"failed sample never publishes");
    a.reset();a.tick(0,ident());a.tick(.25,page("DIR TO"));sample_pages(a,0,plan(),fuel());a.tick(1.8,plan());
    auto bad_data=data();bad_data.fields["cont4w"]="<OTHER";check(a.tick(2.1,bad_data).empty(),"never fire LSK4L on unverified index");
    a.tick(5,bad_data);check(!a.busy()&&a.status()=="RESTORE FAILED"&&!a.plan().observed&&a.plan().alternate.empty(),"restore failure discards tentative alternate");
    a.reset();s=page("INIT");s.fields["label1w"]="ZFW/ZFWCG";s.fields["label2w"]="BLOCK";cycle(a,0,s,plan());
    a.tick(3,s,false);check(!a.plan().observed&&a.plan().alternate.empty(),"power/provider loss invalidates plan and alternate");
    a.reset();cycle(a,0,data(),plan());check(a.tick(5,data()).empty(),"30 second polling interval");
    check(!a.wants_screen(5)&&a.wants_screen(33),"idle sampler does not read all text DataRefs every frame");
    a.reset();a.tick(0,data());a.tick(.25,page("DIR TO"));a.tick(.5,plan());a.tick(.7,plan());
    a.tick(.95,fuel("ZUUU","ZBAA"));a.tick(1.15,fuel());
    check(a.tick(1.25,fuel()).empty(),"changing alternate restarts stability wait");
    check(a.tick(1.4,fuel())=="Fpln","stable new alternate permits verification");
    a.tick(1.65,plan("ZUCK","ZSSS"));a.tick(1.85,plan("ZUCK","ZSSS"));
    check(a.status()=="PLAN CHANGED"&&!a.plan().observed,"cross-page route change discards tentative alternate");
    check(a.tick(2,plan())=="Data","route mismatch restores original page");a.tick(2.25,data());
    check(!a.busy()&&a.plan().alternate.empty(),"route mismatch never publishes alternate");
    a.reset();a.tick(0,data());a.tick(.25,page("DIR TO"));a.tick(.5,plan());a.tick(.7,plan());
    a.tick(.95,fuel("ZSSS"));a.tick(1.15,fuel("ZSSS"));
    check(a.status()=="FUEL PAGE MISMATCH"&&!a.plan().observed,"wrong destination on fuel page is rejected");
    a.reset();a.tick(0,data());a.tick(.25,page("DIR TO"));a.tick(.5,plan());a.tick(.7,plan());
    a.activity(.8);check(!a.busy()&&a.plan().alternate.empty(),"crew input interrupts fuel sampling");
    a.reset();a.tick(0,data());a.tick(.25,page("DIR TO"));a.tick(.5,plan());a.tick(.7,plan());
    a.tick(4,plan());check(a.tick(4.1,plan())=="Data","fuel page timeout restores original page");
}
int main(){try{tests();std::cout<<"PASS "<<checks<<" FMS checks\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
