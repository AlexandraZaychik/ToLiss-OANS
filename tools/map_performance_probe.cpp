#include "a350_map.hpp"
#include "map_viewport.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <stdexcept>

// Offline CPU measurement of the production Release map path, without GL/SDK.
int main(int argc,char** argv) {
    try {
        if(argc<5)throw std::runtime_error("Usage: map_performance_probe ASSETS XPLANE_ROOT ICAO BUILDINGS [FRAMES] [RANGE_NM] [SIDES]");
        const int frames=argc>5?std::stoi(argv[5]):180;
        if(frames<2)throw std::runtime_error("FRAMES must be >= 2");
        std::vector<oans::IndexEntry> index;
        for(const auto& file:oans::scenery_files(std::filesystem::u8path(argv[2]))) {
            auto entries=oans::scan_index(file);index.insert(index.end(),entries.begin(),entries.end());
        }
        const auto entry=oans::find_airport(oans::canonical_index(index),argv[3]);
        if(!entry)throw std::runtime_error("Airport absent");
        std::cout<<"airport="<<entry->icao<<" apt="<<entry->file.u8string()<<" buildings="<<argv[4]
                 <<" frames="<<frames<<" OANS_DEBUG=0 O2; CPU only, no OpenGL or simulator\n";
        oans::A350Map map(std::filesystem::u8path(argv[1]),*entry,std::filesystem::u8path(argv[4]));
        const auto stats=map.stats();
        std::cout<<"pavements="<<stats.pavements<<" stand_labels="<<stats.stand_labels
                 <<" terminals="<<stats.terminals<<" buildings="<<stats.buildings<<'\n';
        std::cout<<"scenario,range_nm,sides,mean_ms,p50_ms,p95_ms,builds,hits,mean_vertices_per_side\n";
        for(double range:{.2,.5,1.})for(int sides:{1,2})for(int scenario=0;scenario<3;++scenario) {
            if((argc>6&&std::abs(range-std::stod(argv[6]))>1e-6)||(argc>7&&sides!=std::stoi(argv[7])))continue;
            oans::A350View base;base.mode=3;base.range=range;
            base.lat=entry->icao=="ZUCK"?29.716054916:map.ref_lat();
            base.lon=entry->icao=="ZUCK"?106.651611328:map.ref_lon();
            base.psi=base.mag=250.77468872;base.angle=base.psi*oans::pi/180;
            const auto vp=oans::map_viewport(base.mode,false);
            base.anchor=vp.anchor;base.radius=vp.radius;base.bottom=vp.bottom;base.scale=vp.radius/(range*1852);
            base.center=oans::local(base.lat,base.lon,map.ref_lat(),map.ref_lon());
            for(int side=0;side<sides;++side)map.render(side,base); // warm up, exclude first build
            std::uint64_t builds=0,hits=0,vertices=0;
            for(int side=0;side<sides;++side){auto c=map.cache_stats(side);builds-=c.builds;hits-=c.hits;}
            std::vector<double> times;
            for(int frame=0;frame<frames;++frame) {
                auto view=base;
                // 15 kt north at 60 frames/s; same geographic pose on both NDs.
                const double distance=scenario?15*oans::kt*(frame+1)/60.:0.;
                view.lat+=distance/oans::earth*180/oans::pi;view.center.y+=distance;
                if(scenario==2){view.psi+=.05*(frame+1);view.mag=view.psi;view.angle=view.psi*oans::pi/180;}
                const auto start=std::chrono::steady_clock::now();
                for(int side=0;side<sides;++side)vertices+=map.render(side,view).size();
                times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
            }
            for(int side=0;side<sides;++side){auto c=map.cache_stats(side);builds+=c.builds;hits+=c.hits;}
            double sum=0;for(double t:times)sum+=t;std::sort(times.begin(),times.end());
            std::cout<<(scenario==0?"stationary":scenario==1?"taxi_straight":"taxi_turn")<<','<<range<<','<<sides
                     <<','<<std::fixed<<std::setprecision(4)<<sum/frames<<','<<times[frames/2]<<','<<times[(frames-1)*95/100]
                     <<','<<builds<<','<<hits<<','<<vertices/(frames*sides)<<std::endl;
        }
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
