#pragma once
#include "fms_source.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace oans::aircraft {
using Bindings=std::vector<std::pair<std::string,std::string>>;
// Values are read on the simulator thread. Adapters own their encodings/units.
struct DataAccess {
    std::function<double(const std::string&,int,double)> value;
    std::function<bool(const std::string&,int)> numeric;
};
struct Backend {
    const char* id;
    const char* manufacturer;
    Bindings bindings;
    bool (*matches)();
    bool (*ready)(const DataAccess&);
    void (*diagnostics)(std::ostream&,const DataAccess&);
    bool (*load_profile)(const std::filesystem::path&,const std::filesystem::path&,
                         const std::string&,std::filesystem::path&,std::string&);
    std::unique_ptr<FmsSource> (*make_fms)();
    int (*nd_mode)(const DataAccess&,int); // normalized NAV=2, ARC=3, PLAN=4
    bool (*position_ready)(const DataAccess&,int); // -1: either side
    bool (*powered)(const DataAccess&);
    const char* (*overlay_blocked)(const DataAccess&,int,bool);
    bool (*advisory_available)(const DataAccess&,int,bool);
    const double* (*uv)(const DataAccess&,int,bool);
    double (*brightness)(const DataAccess&,int,bool);
};
// Only implemented families are registered. ICAO alone does not identify a vendor.
const Backend* detect();
}
