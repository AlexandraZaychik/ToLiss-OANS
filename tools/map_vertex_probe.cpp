#include "a350_map.hpp"
#include "map_viewport.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

// Deterministic output for comparing two implementations. Each vertex field is
// serialized separately, so compiler padding cannot hide or invent differences.
int main(int argc,char** argv) {
    try {
        if(argc!=6)throw std::runtime_error("Usage: map_vertex_probe ASSETS XPLANE_ROOT ICAO BUILDINGS OUTPUT");
        std::vector<oans::IndexEntry> index;
        for(const auto& file:oans::scenery_files(std::filesystem::u8path(argv[2]))) {
            auto entries=oans::scan_index(file);index.insert(index.end(),entries.begin(),entries.end());
        }
        auto entry=oans::find_airport(oans::canonical_index(index),argv[3]);
        if(!entry)throw std::runtime_error("Airport absent");
        oans::A350Map map(std::filesystem::u8path(argv[1]),*entry,std::filesystem::u8path(argv[4]));
        std::ofstream out(std::filesystem::u8path(argv[5]),std::ios::binary);out.exceptions(std::ios::failbit|std::ios::badbit);
        auto value=[&](const auto& v){out.write(reinterpret_cast<const char*>(&v),sizeof v);};
        auto text=[&](const std::string& s){value(std::uint64_t(s.size()));out.write(s.data(),s.size());};
        size_t cases=0,total=0;
        auto vertices=[&](const auto& vs) {
            value(std::uint64_t(vs.size()));++cases;total+=vs.size();
            for(const auto& v:vs){value(v.x);value(v.y);value(v.u);value(v.v);value(v.rgba);}
        };
        for(bool on:{false,true}){const auto& s=map.oans_switch(on);vertices(s.vertices);for(double n:s.bounds)value(n);}
        for(const auto p:{oans::Vec2{300,300},oans::Vec2{-1,601}})vertices(map.cursor(p));
        for(int mode:{2,3,4})for(bool enabled:{false,true}){
            vertices(map.plan_notice(mode,enabled));
            for(bool pfd:{false,true})vertices(map.advisory(mode,pfd,{{"02L","20R",0,0}},"02L",enabled));
        }
        for(int mode:{2,3,4})for(double range:{.2,.5,1.,2.,5.})for(bool panel:{false,true})for(bool detail:{false,true}) {
            oans::A350View v;v.mode=mode;v.range=range;
            v.lat=entry->icao=="ZUCK"?29.716054916:map.ref_lat();v.lon=entry->icao=="ZUCK"?106.651611328:map.ref_lon();
            v.psi=250.77468872;v.mag=248.25;v.angle=mode==4?0:v.psi*oans::pi/180;
            auto vp=oans::map_viewport(mode,panel);v.anchor=vp.anchor;v.radius=vp.radius;v.bottom=vp.bottom;v.scale=vp.radius/(range*1852);
            v.center=oans::local(v.lat,v.lon,map.ref_lat(),map.ref_lon());
            if(detail){v.crosses={v.center};v.flags={v.center+oans::Vec2{20,30}};v.runway_ahead={{"02L","20R",0,0}};
                v.alert_blink=true;v.fms_airport=entry->icao;v.fms_runway="02L";v.simple_stand_names=false;
                v.traffic={{1,v.lat+.0001,v.lon+.0001,45}};}
            vertices(map.render(0,v));
            for(const auto& label:map.runway_labels(0)){
                text(label.first);text(label.second);text(label.selected);
                for(auto p:{label.center,label.start,label.end}){value(p.x);value(p.y);}
            }
            vertices(map.render_base(1,v,0,true));vertices(map.render_dynamic(v));
            // Ensure a subsequent UI reset cannot change retained airport data.
            vertices(map.cursor({380,275}));vertices(map.render(0,v));
            v.center.x+=2;v.angle+=.03;vertices(map.render_base(1,v,.2));
            v.position_valid=false;vertices(map.render_dynamic(v));
        }
        std::cout<<"PASS serialized "<<cases<<" buffers, "<<total<<" vertices plus switch bounds and runway labels\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
