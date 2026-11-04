#include "a350_map.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace oans;
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    try {
        const auto fixture=std::filesystem::u8path(argv[2]);
        std::ofstream f(fixture);
        f<<"1 0 0 0 ZTST Layer test\n100 45 1 0 0.25 0 0 0 09 30.0 120.0 0 0 0 0 0 0 27 30.0 120.035 0 0 0 0 0 0\n99\n";f.close();
        IndexEntry entry;entry.file=fixture;entry.icao="ZTST";
        A350Map map(std::filesystem::u8path(argv[1]),entry);
        A350View view;view.lat=30;view.lon=120.01;view.center=local(view.lat,view.lon,30,120);
        view.scale=.2;view.mode=4;view.traffic={{1,30.001,120.01,90,"TEST","A320",10}};
        const auto all=map.render_dynamic(view);
        auto compass=map.render_compass(view);const auto aircraft=map.render_aircraft(view);
        if(compass.empty()||aircraft.empty())throw std::runtime_error("Missing compass or aircraft layer");
        compass.insert(compass.end(),aircraft.begin(),aircraft.end());
        if(compass.size()!=all.size())throw std::runtime_error("Split layer vertex count differs");
        for(size_t i=0;i<all.size();++i) {
            const auto& a=all[i];const auto& b=compass[i];
            if(a.x!=b.x||a.y!=b.y||a.u!=b.u||a.v!=b.v||a.rgba!=b.rgba)
                throw std::runtime_error("Split layer geometry differs");
        }
        view.position_valid=false;view.traffic.clear();
        if(!map.render_aircraft(view).empty())throw std::runtime_error("Aircraft pass contains non-aircraft geometry");
        if(map.render_compass(view).empty())throw std::runtime_error("Compass pass lost its geometry");
        std::cout<<"PASS dynamic layer == compass + aircraft; aircraft-only pass excludes map and compass\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
