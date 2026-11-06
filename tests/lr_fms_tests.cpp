#include "aircraft/lr/mcdu.hpp"
#include "mcdu_contract_tests.hpp"

using Contract=oans_test::McduContractTests<oans::lr::McduScreen,
    oans::lr::FmsSampler,oans::lr::FmsAutoCapture,oans::lr::McduPage>;
#include "aircraft/lr/mcdu_screen.hpp"

struct LrTests : Contract {
    void lr_screen_tests() {
        std::array<std::string,16> rows{};
        rows[0]="                      ←→";rows[1]="        TIME  SPD/ALT";
        rows[2]="ZUUU02L ---- ---/ 1617";rows[11]="DEST    TIME  DIST  EFOB";
        rows[12]="ZBAA01 0000 1032 ----";rows[13]="                      ↑↓";
        const auto fp=oans::lr::mcdu_screen(rows);
        check(fp.scratchpad_empty(),"LR arrow indicators are not scratchpad input");
        const auto p=parse_fpln(fp);
        check(p.observed&&p.origin.airport=="ZUUU"&&p.origin.runway=="02L"&&
              p.destination.airport=="ZBAA"&&p.destination.runway=="01","LR actual F-PLN endpoints");
        auto fuel_rows=rows;fuel_rows[0]="FUEL PRED";fuel_rows[2]="ZBAA 0000 ---.-";fuel_rows[4]="ZBAD 0008 ---.-";
        auto parsed=p;
        check(parse_fuel_pred(oans::lr::mcdu_screen(fuel_rows),parsed)&&parsed.alternate=="ZBAD","LR actual alternate");
        fuel_rows[2]="ZSSS 0000 ---.-";
        check(!parse_fuel_pred(oans::lr::mcdu_screen(fuel_rows),parsed),"LR mismatched fuel page rejected");
        rows[2]="ABCD ---- ---/-----";
        check(parse_fpln(oans::lr::mcdu_screen(rows)).origin.airport.empty(),"LR four-letter fix never inferred as origin");
        rows[2]="ZUUU02L ---- ---/1617";rows[0]="TMPY";
        check(!parse_fpln(oans::lr::mcdu_screen(rows)).observed,"LR temporary route rejected");
        rows[13]="USER INPUT ↑↓";
        check(!oans::lr::mcdu_screen(rows).scratchpad_empty(),"LR crew input preserved");
        rows[13]="";rows[14]="MESSAGE";
        check(!oans::lr::mcdu_screen(rows).scratchpad_empty(),"LR additional message blocks navigation");
        rows={};rows[0]="DATA INDEX 1/2 ←→";
        check(mcdu_page(oans::lr::mcdu_screen(rows))==McduPage::data,"LR DATA page counter normalized");
        rows[0]="INIT ←→";rows[1]="CO RTE FROM/TO";
        check(mcdu_page(oans::lr::mcdu_screen(rows))==McduPage::init_a,"LR INIT arrows normalized");
        FmsSampler a;cycle(a,0,data(),fp,oans::lr::mcdu_screen(std::array<std::string,16>{
            "FUEL PRED"," AT TIME EFOB","ZBAA 0000 ---.-","","ZBAD 0008 ---.-"}));
        check(a.plan().observed&&a.plan().alternate=="ZBAD","LR screen adapter completes shared verified transaction");
    }
};
int main() {
    try {
        LrTests suite;suite.lr_screen_tests();suite.run();
        std::cout<<"PASS "<<suite.checks<<" LR FMS checks\n";
    }catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
