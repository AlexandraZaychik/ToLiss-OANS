#pragma once
#include "fms.hpp"
#include "test_support.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

// Both aircraft own their sampler implementation. Run the same behavioral
// contract against each, while keeping aircraft-specific screen tests separate.
namespace oans_test {
using oans::FmsPlan;
template<class Screen,class Sampler,class AutoCapture,class Page>
struct McduContractTests {
    using McduScreen=Screen;
    using FmsSampler=Sampler;
    using FmsAutoCapture=AutoCapture;
    using McduPage=Page;
    std::string family;
    void case_start(const std::string& name){scenario(family+"/"+name);}
    int checks=0;
    void check(bool v,const char* text){++checks;if(!v)throw std::runtime_error(text);}
    static McduScreen plan(std::string from="ZUCK02L",std::string to="ZUUU02L") {
        McduScreen s;s.fields={{"stitlew","  FROM "},{"label6w","DEST    TIME  DIST  EFOB"},
            {"cont1g",from},{"cont6w",to+" 0038 213 11.4"},{"titles","                      23"}};return s;
    }
    static McduScreen page(const std::string& title){McduScreen s;s.fields["titlew"]=title;return s;}
    static McduScreen data(){auto s=page("DATA INDEX");s.fields["stitlew"]="1/2";s.fields["titles"]="                      23";s.fields["cont4w"]="<A/C STATUS";return s;}
    static McduScreen ident(){auto s=page("A320-251N");s.fields["label2w"]="ACTIVE NAV DATA BASE";s.fields["label3w"]="SECOND NAV DATA BASE";return s;}
    static McduScreen fuel(std::string dest="ZUUU",std::string alternate="ZALT") {
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
    void run() {
        case_start("fms/verified-ident-transaction");
        FmsSampler a(false);cycle(a,0,ident(),plan());
        check(a.plan().origin.airport=="ZUCK"&&a.plan().origin.runway=="02L"&&a.plan().destination.airport=="ZUUU"&&a.plan().alternate=="ZALT","publish complete plan only after verified IDENT restoration");

        case_start("fms/crew-interruption");
        a.request();a.tick(5,data());a.activity(5.1,false);
        check(!a.busy()&&a.cancel().empty()&&!a.wants_screen(1000),"never restore or retry over crew input");

        case_start("fms/unverified-lsk-restore");
        a.reset();a.tick(0,ident());a.tick(.25,page("DIR TO"));sample_pages(a,0,plan(),fuel());a.tick(1.8,plan());
        auto bad=data();bad.fields["cont4w"]="<OTHER";
        check(a.tick(2.1,bad).empty(),"never issue LSK4L on unverified page");
        a.tick(5,bad);check(!a.busy()&&!a.plan().observed&&a.status()=="RESTORE FAILED","failed restoration discards tentative plan");

        case_start("fms/cross-page-route-change");
        a.reset();a.tick(0,data());a.tick(.25,page("DIR TO"));a.tick(.5,plan());a.tick(.7,plan());a.tick(.95,fuel());a.tick(1.15,fuel());
        a.tick(1.65,plan("ZUCK","ZSSS"));a.tick(1.85,plan("ZUCK","ZSSS"));
        check(!a.plan().observed&&a.status()=="PLAN CHANGED","inconsistent F-PLN/FUEL transaction never publishes");

        case_start("fms/scratchpad-retry-budget");
        a.reset(false);a.activity(0,true);auto blocked=data();blocked.fields["spw"]="INPUT";
        check(!a.wants_screen(4.99)&&a.tick(5,blocked).empty()&&!a.busy(),"crew input blocks navigation after cooldown");
        check(!a.wants_screen(9.99)&&a.wants_screen(10),"one delayed retry");
        a.tick(10,blocked);check(!a.wants_screen(1000),"retry budget is bounded");

        case_start("fms/partial-pair-and-flight-phase");
        a.reset(false);cycle(a,0,data(),plan("ZUCK02L",""));
        FmsAutoCapture automatic;automatic.observe(a.sampled_plan());
        cycle(a,5,data(),plan("PPOS","ZUUU02L"));automatic.observe(a.sampled_plan());
        check(a.plan().origin.airport=="ZUCK"&&a.plan().destination.airport=="ZUUU"&&!automatic.complete(),"cached halves are not one complete capture");
        a.set_flight_phase(5);cycle(a,10,data(),plan("ZBAA","ZSSS"),fuel("ZSSS","ZSPD"));
        check(a.plan().origin.airport=="ZUCK"&&a.plan().origin.runway=="02L"&&a.plan().destination.airport=="ZSSS","takeoff freezes origin, not destination");

        case_start("fms/direct-init-independent-of-automation");
        a.reset(false);auto init=page("INIT");init.fields["label1w"]="CO RTE FROM/TO";init.fields["label2w"]="ALTN/CO RTE";
        init.fields["cont1b"]=std::string(15,' ')+"ZBAA/ZSSS";init.fields["cont2b"]="ZSPD/----------";init.fields["spw"]="USER INPUT";
        check(a.capture_init(0,init)&&a.plan().origin.airport=="ZBAA"&&a.plan().destination.airport=="ZSSS"&&a.plan().alternate=="ZSPD","INIT R1 reads directly without changing pages or scratchpad");
        init.fields["cont1b"]=std::string(15,' ')+"ZUCK/ZUUU";init.fields["cont2b"]="ZSPD/----------";
        check(a.capture_init(.1,init)&&a.plan().alternate=="ZSPD"&&a.plan().origin.airport=="ZUCK"&&a.plan().destination.airport=="ZUUU"&&a.complete_pair()&&!a.wants_screen(1000),"INIT L2 reads all airports and does not schedule page navigation");

        init.fields["cont2b"]="INVALID/----------";
        init.fields["cont1b"]=std::string(15,' ')+"ZUCK/ZSSS";
        check(a.capture_init(.2,init)&&a.plan().destination.airport=="ZSSS"&&a.plan().alternate.empty(),"invalid alternate does not block destination change or retain stale alternate");
        init.fields["cont1b"]="----/----";init.fields["cont2b"]="ZSPD/----------";
        check(a.capture_init(.3,init)&&a.plan().destination.airport=="ZSSS"&&a.plan().alternate=="ZSPD"&&!a.complete_pair(),"valid alternate updates independently of unavailable endpoints");
        init.fields["cont2b"]="NONE/----------";
        check(a.capture_init(.4,init)&&a.plan().alternate.empty(),"explicit NONE clears alternate");
        cycle(a,5,data(),plan(),fuel());a.set_flight_phase(5);
        init.fields["cont1b"]=std::string(15,' ')+"ZBAA/ZSSS";init.fields["cont2b"]="ZSPD/----------";
        check(a.capture_init(8,init)&&a.plan().origin.airport=="ZUCK"&&a.plan().origin.runway=="02L"&&a.plan().destination.airport=="ZSSS"&&a.plan().alternate=="ZSPD","whole INIT read preserves frozen origin and updates destination and alternate");

        case_start("fms/temporary-fpln-rejected");
        auto draft=plan();draft.fields["titley"]="TMPY";
        check(!parse_fpln(draft).observed,"temporary F-PLN is never active");
        case_start("fms/temporary-init-rejected");
        init.fields["titley"]="TMPY";const auto generation=a.generation();
        check(!a.capture_init(1,init)&&a.generation()==generation,"temporary INIT cannot overwrite active endpoints");

        case_start("fms/automatic-phase-gate-and-latch");
        automatic=FmsAutoCapture{};automatic.update_phase(0,1);
        check(!automatic.request_due(0,true,false),"preflight phase does not flip pages");
        automatic.update_phase(3,2);
        check(!automatic.request_due(3,false,false)&&!automatic.request_due(3,true,true)&&automatic.request_due(3,true,false),"power and busy gates before automatic capture");
        check(!automatic.request_due(62.99,true,false)&&automatic.request_due(63,true,false),"automatic retry interval");
        automatic.observe(parse_fpln(plan()));check(automatic.complete()&&!automatic.request_due(10000,true,false),"one complete pair stops polling");

        case_start("fms/empty-plan-clears-stale-data");
        a.reset(false);cycle(a,0,data(),plan());cycle(a,5,data(),plan("PPOS",""));
        check(a.plan().observed&&a.plan().origin.airport.empty()&&a.plan().destination.airport.empty()&&a.plan().alternate.empty(),"confirmed empty route clears prior airports and alternate");
    }
};
}
