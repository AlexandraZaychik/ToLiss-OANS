#include "a350_map.hpp"
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv) {
    try {
        if(argc!=4)throw std::runtime_error("Expected assets, apt.dat and ICAO");
        const auto index=oans::scan_index(std::filesystem::u8path(argv[2]));
        const auto entry=oans::find_airport(index,argv[3]);
        if(!entry)throw std::runtime_error("Airport missing");
        const auto airport=oans::load_airport(*entry);
        const auto& runway=airport.runways.at(0);
        oans::A350Map map(std::filesystem::u8path(argv[1]),*entry);
        oans::A350View view;
        view.center=oans::local(airport.lat,airport.lon,map.ref_lat(),map.ref_lon())+(runway.start+runway.end)*.5;
        view.scale=.2;view.position_valid=false;view.fms_airport=entry->icao;
        int checks=0;
        for(int mode:{2,3,4})for(const auto& manual:{std::string{},runway.name,runway.reciprocal}) {
            view.mode=mode;view.map_runway=manual;view.fms_runway=runway.name;
            const auto expected=manual.empty()?runway.name:manual;
            map.render(0,view);
            bool found=false;
            for(const auto& label:map.runway_labels(0))if(label.selected==expected)found=true;
            if(!found)throw std::runtime_error("Map selected endpoint differs from manual/FMS resolution");
            const auto& vertices=map.advisory(mode,false,{{runway.name,runway.reciprocal,0,0}},expected,true);
            bool green=false;
            for(const auto& v:vertices)if(v.rgba==0xFF00FF00)green=true;
            if(mode!=4&&!green)throw std::runtime_error("Selected advisory endpoint is not green");
            ++checks;
        }
        std::cout<<"PASS "<<checks<<" manual/FMS/conflicting endpoint cases across NAV/ARC/PLAN\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
