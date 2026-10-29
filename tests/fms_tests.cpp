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
    a.request();
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
void automatic_tests() {
    for(int phase=1;phase<=10;++phase) {
        FmsAutoCapture a;
        check(a.phase_due(0),"phase is read immediately after initialization");
        a.update_phase(10,phase);
        const double interval=phase<=4?5:phase<=7?30:60;
        check(!a.phase_due(10+interval-.01)&&a.phase_due(10+interval),"phase-specific polling interval");
        check(a.request_due(10,true,false)==(phase>=2&&phase<5),"automatic capture is limited to ECAM phases two through four");
    }
    for(int phase:{-1,0,11,99}) {
        FmsAutoCapture a;a.update_phase(0,phase);
        check(a.phase()==0&&!a.phase_due(4.99)&&a.phase_due(5),"missing or invalid phase retries in five seconds");
        check(!a.request_due(0,true,false)&&!a.request_due(100,true,false),"invalid phase never triggers automatic page changes");
    }
    FmsAutoCapture a;a.update_phase(0,1);
    check(!a.request_due(0,true,false),"phase one does not sample automatically");
    a.update_phase(5,2);
    check(!a.request_due(5,false,false)&&!a.request_due(5,true,true),"power loss and busy transactions defer automatic request");
    check(a.request_due(5,true,false),"phase two starts automatic capture immediately");
    check(!a.request_due(64.99,true,false)&&a.request_due(65,true,false),"automatic attempts are sixty seconds apart");
    a.update_phase(66,0);
    check(!a.request_due(125,true,false),"invalid new phase gates an overdue attempt");
    a.update_phase(126,8);check(!a.request_due(126,true,false),"phase eight never starts automatic capture");
    a.update_phase(127,4);check(a.request_due(127,true,false),"phase four permits an overdue capture");
    FmsPlan p;p.observed=true;p.origin.airport="ZUCK";
    a.observe(p);a.observe(p);
    check(!a.complete()&&a.confirmations()==0,"two identical origin-only samples do not latch");
    p.origin.airport.clear();p.destination.airport="ZUUU";a.observe(p);a.observe(p);
    check(!a.complete()&&a.confirmations()==0,"two identical destination-only samples do not latch");
    p.origin.airport="ZUCK";a.observe(p);
    check(a.complete()&&a.confirmations()==1,"one full airport pair completes automatic capture");
    p.destination.airport="ZSSS";a.observe(p);
    check(a.complete()&&a.confirmations()==1,"later airport changes do not restart completed capture");
    a.observe(FmsPlan{});a.observe(p);
    check(a.complete()&&a.confirmations()==1,"later failures preserve completion latch");
    auto empty=p;empty.origin.airport.clear();empty.destination.airport.clear();a.observe(empty);a.observe(p);
    check(a.complete()&&a.confirmations()==1,"later empty plans preserve completion latch");
    p.origin.runway="02L";p.destination.runway="18R";a.observe(p);
    check(a.complete()&&a.confirmations()==1,"runways are not required for capture completion");
    a.observe(FmsPlan{});
    check(a.complete()&&!a.request_due(10000,true,false),"later failures never restart completed automatic session");
    a=FmsAutoCapture{};a.update_phase(0,2);
    check(!a.complete()&&a.confirmations()==0&&a.request_due(0,true,false),"session reset restarts capture");
    FmsSampler manual(false);
    check(!manual.wants_screen(0)&&manual.tick(0,ident()).empty(),"automatic port can suppress ungated initial sample");
    manual.activity(0);
    check(!manual.wants_screen(4.99)&&manual.wants_screen(5)&&manual.tick(5,ident())=="DirTo","manual input triggers after five seconds without phase gating");
    manual.reset(false);check(!manual.wants_screen(100),"port reset does not bypass phase gate");
    manual.activity(100,false);
    check(!manual.wants_screen(103),"ordinary keys defer but never request sampling");
    manual.activity(104,true);manual.activity(106,false);
    check(!manual.wants_screen(110.99)&&manual.wants_screen(111),"ordinary keys extend pending selected-key cooldown");
    auto scratch=ident();scratch.fields["spa"]="CHECK INPUT";
    check(manual.tick(111,scratch).empty()&&!manual.busy(),"amber MCDU2 scratchpad blocks selected-key check");
    manual.activity(112,true);
    check(manual.tick(117,ident())=="DirTo","selected key can retry after scratchpad is cleared");
    manual.activity(117.1,false);
    check(!manual.busy()&&!manual.wants_screen(122.09)&&manual.wants_screen(122.1),"crew interruption rearms capture after five quiet seconds");
    FmsAutoCapture crew;
    crew.observe(parse_fpln(plan("ZUCK","")));
    check(!crew.complete(),"partial crew capture keeps automatic fallback available");
    manual.reset(false);manual.activity(120,true);
    cycle(manual,125,ident(),plan());
    check(manual.crew_triggered(),"completed transaction retains selected-key source");
    crew.observe(manual.sampled_plan());
    check(crew.complete()&&crew.phase_due(10000)&&!crew.request_due(10000,true,false),"one complete crew capture stops automatic requests while phase tracking continues");
    cycle(manual,130,ident(),plan());
    check(!manual.crew_triggered(),"later automatic request does not inherit crew source");
}
void tests() {
    automatic_tests();
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
    s=plan();s.fields["stitlew"]="  FROM         CQH5482";p=parse_fpln(s);
    check(p.origin.airport=="ZUCK"&&p.destination.airport=="ZUUU","FROM header with live flight number still identifies origin");
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
    auto data_subtitle=data();data_subtitle.fields["stitlew"]="2/2";
    check(mcdu_page(data_subtitle)==McduPage::data,"DATA INDEX remains DATA regardless of subtitle");
    check(std::string(mcdu_page_name(McduPage::data))=="DATA","DATA INDEX uses DATA as its page name");
    auto empty=page("");empty.fields["cont1g"]="PPOS";empty.fields["cont2w"]="F-PLN DISCONTINUITY";
    p=parse_fpln(empty);check(p.observed&&p.reason=="NO FLIGHT PLAN","empty PPOS plan without destination footer");
    FmsSampler a;cycle(a,0,ident(),plan());check(a.plan().destination.runway=="02L"&&a.plan().alternate=="ZALT","IDENT round trip publishes endpoints and alternate");
    cycle(a,40,page("FUEL PRED"),plan("ZUCK","ZUUU"),fuel("ZUUU","----"));check(a.plan().destination.runway.empty()&&a.plan().alternate.empty(),"new sample clears deselected runway and alternate");
    cycle(a,80,page("DIR TO"),empty);check(a.plan().observed&&a.plan().destination.airport.empty()&&a.plan().alternate.empty(),"new empty plan clears old airports and alternate");
    cycle(a,120,plan(),plan());check(a.plan().observed,"sampling from F-PLN still resets through detour");
    const auto saved_generation=a.generation();
    for(double t:{153.,220.,3600.}) {
        check(!a.wants_screen(t)&&a.tick(t,page("UNKNOWN")).empty()&&!a.busy(),"elapsed time never triggers idle sampling");
        check(a.plan().observed&&a.plan().destination.airport=="ZUUU"&&a.generation()==saved_generation,"cached plan does not expire with age");
    }
    a.activity(3600);
    cycle(a,3605,data(),plan("ZBAA","ZSSS"),fuel("ZSSS","----"));
    check(a.plan().destination.airport=="ZSSS","crew input still updates cached plan after long idle");
    for(const char* title:{"UNKNOWN","PERF","PROG","ATC"}) {
        a.reset();cycle(a,0,page(title),plan());
        check(a.plan().observed&&a.plan().alternate=="ZALT","recognized and unknown original pages publish after restoration");
    }
    a.reset();a.tick(0,page("UNKNOWN"));a.tick(4,page("UNKNOWN"));
    check(a.tick(4.1,page("UNKNOWN"))=="Fpln","unsupported original page timeout falls back to F-PLN");
    a.tick(4.4,plan());check(!a.busy()&&!a.plan().observed,"failed sample from unsupported page never publishes");
    a.reset();a.tick(0,page("UNKNOWN"));a.activity(.1);
    check(!a.busy()&&a.tick(.5,page("UNKNOWN")).empty(),"crew interrupts unsupported-page sample without restore command");
    a.reset();s=ident();s.fields["spw"]="ZBAA";check(a.tick(0,s).empty(),"scratchpad input defers sampling");
    a.reset();a.activity(0);check(!a.wants_screen(4.9)&&a.tick(4.9,ident()).empty(),"crew cooldown defers reads and sampling");
    a.activity(2);check(!a.wants_screen(6.9)&&a.tick(6.9,ident()).empty(),"each crew input restarts five second cooldown");
    check(a.wants_screen(7)&&a.tick(7,ident())=="DirTo","resume exactly five seconds after last input");
    a.activity(7.1);check(!a.busy()&&!a.plan().observed&&a.tick(7.4,page("DIR TO")).empty(),"crew interruption never fights back with restore keys");
    a.reset();a.tick(0,ident());a.tick(4,ident());check(a.tick(4.1,ident())=="Data","page timeout attempts restoration");
    a.tick(4.4,data());a.tick(4.7,ident());check(!a.busy()&&!a.plan().observed,"failed sample never publishes");
    a.reset();a.tick(0,ident());a.tick(.25,page("DIR TO"));sample_pages(a,0,plan(),fuel());a.tick(1.8,plan());
    auto bad_data=data();bad_data.fields["cont4w"]="<OTHER";check(a.tick(2.1,bad_data).empty(),"never fire LSK4L on unverified index");
    a.tick(5,bad_data);check(!a.busy()&&a.status()=="RESTORE FAILED"&&!a.plan().observed&&a.plan().alternate.empty(),"restore failure discards tentative alternate");
    a.reset();s=page("INIT");s.fields["label1w"]="ZFW/ZFWCG";s.fields["label2w"]="BLOCK";cycle(a,0,s,plan());
    a.tick(3,s,false);check(!a.plan().observed&&a.plan().alternate.empty(),"power/provider loss invalidates plan and alternate");
    a.reset();cycle(a,0,data(),plan());check(a.tick(5,data()).empty(),"completed sample waits for input");
    check(!a.wants_screen(5)&&!a.wants_screen(33),"idle sampler does not periodically read text DataRefs");
    a.activity(33);
    check(!a.wants_screen(37.9)&&a.wants_screen(38)&&a.tick(38,data())=="DirTo","crew input triggers sampling after five seconds without polling");
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
void partial_and_phase_tests() {
    for(bool crew:{false,true}) {
        FmsSampler a(false);FmsAutoCapture automatic;
        if(crew)a.activity(0,true);
        cycle(a,5,data(),plan("ZUCK02L",""));
        check(a.plan().origin.airport=="ZUCK"&&a.plan().destination.airport.empty()&&!a.complete_pair(),
              "origin-only capture publishes immediately for either trigger");
        automatic.observe(a.sampled_plan());
        check(!automatic.complete(),"origin-only capture keeps automatic fallback available");
        if(crew)a.activity(10,true);
        cycle(a,15,data(),plan("PPOS","ZUUU02L"));
        check(a.plan().origin.airport=="ZUCK"&&a.plan().origin.runway=="02L"&&a.plan().destination.airport=="ZUUU"&&!a.complete_pair(),
              "destination-only capture fills destination and retains cached origin");
        automatic.observe(a.sampled_plan());
        check(!automatic.complete(),"merged halves never masquerade as one complete capture");
        if(crew)check(a.wants_screen(22.05),"partial crew capture still retries despite both cached airports being present");
        cycle(a,25,data(),plan("ZBAA",""));
        check(a.plan().origin.airport=="ZBAA"&&a.plan().origin.runway.empty()&&a.plan().destination.airport=="ZUUU"&&a.plan().alternate=="ZALT",
              "origin-only update retains destination and alternate while clearing deselected origin runway");
    }
    FmsSampler a(false);
    cycle(a,0,data(),plan());
    a.set_flight_phase(5);a.activity(3,true);
    cycle(a,8,data(),plan("ZBAA18R","ZSSS36L"),fuel("ZSSS","ZSPD"));
    check(a.plan().origin.airport=="ZUCK"&&a.plan().origin.runway=="02L"&&a.plan().destination.airport=="ZSSS"&&a.plan().alternate=="ZSPD",
          "phase five freezes origin and departure runway while destination and alternate update");
    a.set_flight_phase(8);cycle(a,20,data(),plan("PPOS","ZBAA"),fuel("ZBAA","----"));
    check(a.plan().origin.airport=="ZUCK"&&a.plan().destination.airport=="ZBAA"&&a.plan().alternate.empty(),
          "later phases preserve origin when FROM disappears");
    a.activity(23,true);a.tick(28,data());a.activity(28.1,false);
    check(a.plan().origin.airport=="ZUCK"&&a.plan().origin.runway=="02L"&&a.plan().observed,
          "crew interruption cannot clear frozen origin");
    cycle(a,40,data(),plan("PPOS",""));
    check(a.plan().origin.airport=="ZUCK"&&a.plan().destination.airport.empty(),"empty in-flight plan cannot clear frozen origin");
    a.tick(43,data(),false);
    check(a.plan().origin.airport=="ZUCK"&&a.plan().observed,"provider loss cannot clear frozen origin");
    a.reset(false);a.set_flight_phase(5);cycle(a,0,data(),plan());
    check(a.plan().origin.airport.empty()&&a.plan().destination.airport=="ZUUU"&&a.complete_pair(),
          "phase five does not fill even an initially empty origin but retains raw capture completeness");
    a.reset(false);cycle(a,0,data(),plan());
    a.request();a.tick(5,data());a.tick(5.25,page("DIR TO"));
    sample_pages(a,5,plan("ZBAA","ZSSS"),fuel("ZSSS"));
    a.set_flight_phase(5);a.tick(6.8,plan());a.tick(7.05,data());
    check(a.plan().origin.airport=="ZUCK"&&a.plan().destination.airport=="ZSSS",
          "phase at publication protects origin when phase changes during sampling");
    a.set_flight_phase(4);cycle(a,10,data(),plan("ZBAA","ZSSS"),fuel("ZSSS"));
    check(a.plan().origin.airport=="ZBAA","pre-takeoff phase permits origin updates again");
}
void retry_tests() {
    auto blocked=data();blocked.fields["spw"]="INPUT";
    FmsSampler a(false);a.activity(0,true);
    for(unsigned attempt=0;attempt<=1;++attempt) {
        const double now=5+attempt*5;
        a.request(); // The automatic scheduler must not shorten a crew retry delay.
        check(!a.wants_screen(now-.01)&&a.wants_screen(now),"crew attempts are separated by five seconds");
        check(a.tick(now,blocked).empty()&&!a.busy()&&a.crew_triggered()&&a.crew_retry()==attempt,
              "blocked checks retain crew source and retry number");
    }
    check(!a.wants_screen(100),"one initial attempt plus one retry exhausts the crew request");
    a.activity(100,true);a.tick(105,blocked);
    check(a.crew_retry()==0&&!a.wants_screen(109.99)&&a.wants_screen(110),"new selected key resets exhausted retry budget");
    check(a.tick(110,ident())=="DirTo"&&a.crew_retry()==1,"first retry starts with the original crew source");
    a.activity(110.1,false);
    check(!a.busy()&&!a.wants_screen(115.09)&&a.wants_screen(115.1),"interruption restarts five second quiet period");
    a.tick(115.1,blocked);
    check(a.crew_retry()==0,"ordinary key interrupting a crew transaction refreshes its retry budget");
    cycle(a,120.1,data(),plan());
    check(a.crew_triggered()&&a.crew_retry()==1&&!a.wants_screen(1000),"successful retry stops future retries");

    a.reset(false);a.activity(0,true);
    auto draft=data();draft.fields["titley"]="TMPY";a.tick(5,draft);
    check(!a.wants_screen(9.99)&&a.wants_screen(10),"temporary plan failure also schedules a five second retry");
    a.activity(6,false);
    check(!a.wants_screen(10.99)&&a.wants_screen(11),"ordinary keys defer a pending retry without starting an unrelated capture");

    a.reset(false);a.activity(0,true);cycle(a,5,data(),plan("PPOS",""));
    check(!a.wants_screen(12.04)&&a.wants_screen(12.05),"empty-plan result retries five seconds after restoration completes");
    a.reset(false);a.activity(0,true);a.tick(5,ident());a.tick(9,ident());
    check(a.tick(9.1,ident())=="Data","timed-out crew attempt restores before retrying");
    check(a.tick(9.4,data())=="LSK4L","timed-out untouched screen still restores A/C STATUS");
    a.tick(9.7,ident());
    check(!a.wants_screen(14.69)&&a.wants_screen(14.7),"timeout retry starts five seconds after restoration");

    a.reset(false);a.activity(0,false);a.request();a.stop_automatic_capture();
    check(!a.wants_screen(10),"phase cutoff drops automatic requests waiting for crew idle");
    a.reset();a.tick(0,ident());a.stop_automatic_capture();
    check(a.tick(.25,page("DIR TO"))=="Data","phase cutoff restores an active automatic transaction");
    a.tick(.5,data());a.tick(.75,ident());
    check(!a.busy()&&!a.plan().observed&&!a.wants_screen(100),"phase cutoff neither publishes nor retries an automatic transaction");
    a.reset(false);a.activity(0,true);a.stop_automatic_capture();
    check(a.tick(5,ident())=="DirTo","phase cutoff preserves pending crew requests");
    a.stop_automatic_capture();
    check(a.tick(5.25,page("DIR TO"))=="Fpln","phase cutoff preserves active crew transactions");
}
void tracked_parent_tests() {
    struct Case {McduPage parent;const char* command;McduScreen root;};
    auto init=page("INIT");init.fields["label1w"]="CO RTE";
    const Case cases[]={
        {McduPage::data,"Data",data()},{McduPage::dir,"DirTo",page("DIR TO")},
        {McduPage::init_a,"Init",init},{McduPage::fpln,"Fpln",plan()},
        {McduPage::fuel,"FuelPred",fuel()},{McduPage::radnav,"RadNav",page("RADIO NAV")},
        {McduPage::menu,"Menu",page("MCDU MENU")},{McduPage::sec,"SecFpln",page("SEC INDEX")},
        {McduPage::perf,"Perf",page("TAKE OFF")},{McduPage::prog,"Prog",page("A320 PROG")},
        {McduPage::atc,"ATC",page("ATSU DATALINK")},{McduPage::airport,"Airport",plan()}};
    for(const auto& c:cases) {
        FmsSampler a;
        check(a.tick(0,ident(),true,c.parent)==(c.parent==McduPage::dir?"Data":"DirTo"),"tracked parent overrides visible subpage");
        check(a.saved_page()==c.parent,"snapshot tracked MCDU2 parent before sampling");
        a.tick(.25,c.parent==McduPage::dir?data():page("DIR TO"));
        sample_pages(a,0,plan(),fuel());
        check(a.tick(1.8,plan())==c.command,"restore via recorded parent key");
        check(!a.plan().observed,"never publish before parent restoration");
        a.tick(2.05,page("UNRELATED"),true,McduPage::data);
        check(a.busy()&&!a.plan().observed,"wrong restoration screen cannot publish or change snapshot");
        a.tick(2.3,c.root);
        check(!a.busy()&&a.plan().observed,"publish only after parent screen confirms restoration");
        a.reset();a.tick(0,ident(),true,c.parent);
        check(a.cancel()==c.command,"cancellation also restores recorded parent");
        a.reset();a.tick(0,ident(),true,c.parent);a.activity(.1,false);
        check(a.cancel().empty(),"crew interruption never restores over user input");
    }
}
int main(){try{tests();partial_and_phase_tests();retry_tests();tracked_parent_tests();std::cout<<"PASS "<<checks<<" FMS checks\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
