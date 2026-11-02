#pragma once
#include <string>

namespace oans {
struct FmsEndpoint {std::string airport,runway;};
struct FmsPlan {
    FmsEndpoint origin,destination;
    std::string alternate;
    std::string reason="NOT SAMPLED";
    bool observed=false;
};
bool airport_outside_active_plan(const FmsPlan& plan,const std::string& airport);
}
