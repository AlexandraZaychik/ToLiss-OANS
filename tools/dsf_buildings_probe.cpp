#include "dsf_buildings.hpp"
#include <iostream>
#include <chrono>
int main(int argc,char** argv) {
    if(argc!=4){std::cerr<<"Usage: dsf_buildings_probe XPLANE_ROOT ICAO OUTPUT_CACHE\n";return 2;}
    try {
        std::atomic<bool> cancel{false};std::vector<oans::IndexEntry> entries;
        const auto root=std::filesystem::u8path(argv[1]);
        for(const auto& p:oans::scenery_files(root)) {
            auto part=oans::scan_index(p);entries.insert(entries.end(),part.begin(),part.end());
        }
        const auto entry=oans::find_airport(entries,argv[2]);
        if(!entry)throw std::runtime_error("airport not found");
        const auto start=std::chrono::steady_clock::now();
        const auto r=oans::extract_runtime_dsf(root,*entry,std::filesystem::u8path(argv[3]),cancel);
        std::cout<<r.note<<"\n"<<r.cache.u8string()<<"\nextraction_ms="
                 <<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<"\n";
        return r.buildings?0:1;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
