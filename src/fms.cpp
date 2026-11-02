#include "fms.hpp"

namespace oans {
bool airport_outside_active_plan(const FmsPlan& plan,const std::string& airport) {
    return plan.observed&&!airport.empty()&&
        airport!=plan.origin.airport&&airport!=plan.destination.airport&&airport!=plan.alternate;
}
}
