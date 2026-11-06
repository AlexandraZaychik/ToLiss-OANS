#include "aircraft/toliss/mcdu.hpp"
#include "mcdu_contract_tests.hpp"

using Contract=oans_test::McduContractTests<oans::toliss::McduScreen,
    oans::toliss::FmsSampler,oans::toliss::FmsAutoCapture,oans::toliss::McduPage>;

int main() {
    try {
        Contract suite;suite.run();
        std::cout<<"PASS "<<suite.checks<<" ToLiss FMS checks\n";
    }catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
