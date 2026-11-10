#include "fms_light_tests.hpp"
#include "aircraft/toliss/mcdu.hpp"
#include "mcdu_contract_tests.hpp"

using Contract=oans_test::McduContractTests<oans::toliss::McduScreen,
    oans::toliss::FmsSampler,oans::toliss::FmsAutoCapture,oans::toliss::McduPage>;

namespace toliss_tests {
int run() {
    try {
        Contract suite;suite.family="toliss";suite.run();
        std::cout<<"PASS "<<suite.checks<<" ToLiss FMS checks\n";return 0;
    }catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}

}
#include "aircraft/lr/mcdu.hpp"
#include "aircraft/lr/mcdu_screen.hpp"
namespace lr_tests {
using Contract=oans_test::McduContractTests<oans::lr::McduScreen,oans::lr::FmsSampler,oans::lr::FmsAutoCapture,oans::lr::McduPage>;
int run(){try{Contract suite;suite.family="lr";suite.run();
    scenario("lr/screen-adapter");
    std::array<std::string,16> rows{};rows[0]="                      ←→";rows[1]="        TIME  SPD/ALT";
    rows[2]="ZUUU02L ---- ---/ 1617";rows[11]="DEST    TIME  DIST  EFOB";rows[12]="ZBAA01 0000 1032 ----";rows[13]="                      ↑↓";
    const auto screen=oans::lr::mcdu_screen(rows);const auto p=oans::lr::parse_fpln(screen);
    verify(screen.scratchpad_empty()&&p.observed&&p.origin.airport=="ZUUU"&&p.destination.airport=="ZBAA","real LR row adapter preserves endpoints and ignores arrows");
    return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
}
int main(){test_fms_lights();if(toliss_tests::run())return 1;return lr_tests::run();}
