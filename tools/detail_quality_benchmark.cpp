#include "a350_map.hpp"
#include "map_viewport.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

// Production CPU rendering path. Time advances deterministically at 60 Hz;
// there is no sleeping, simulator, OpenGL submission or network access.
int main(int argc,char** argv) {
    try {
        if(argc!=6)throw std::runtime_error("Usage: detail_quality_benchmark ASSETS XPLANE_ROOT BUILDINGS OUTPUT_CSV REPEATS");
#ifdef _WIN32
        // Keep all comparisons on one logical processor; never change the host's policy.
        if(!SetThreadAffinityMask(GetCurrentThread(),1))throw std::runtime_error("CPU affinity failed");
#endif
        constexpr double seconds=30,speed=15*oans::kt,distance=seconds*speed;
        constexpr int hz=60,frames=1800;
        const int repeats=std::stoi(argv[5]);
        std::vector<oans::IndexEntry> index;
        for(const auto& file:oans::scenery_files(std::filesystem::u8path(argv[2]))) {
            auto part=oans::scan_index(file);index.insert(index.end(),part.begin(),part.end());
        }
        auto entry=oans::find_airport(oans::canonical_index(index),"ZGGG");
        if(!entry)throw std::runtime_error("ZGGG missing");
        const auto airport=oans::load_airport(*entry);
        const auto arp=oans::local(airport.reference.lat,airport.reference.lon,airport.lat,airport.lon);
        oans::Vec2 from,to;std::string route;double best=1e100,edge_length=0;
        for(const auto& e:airport.edges) {
            if(e.runway||e.name.empty()||!airport.nodes.count(e.a)||!airport.nodes.count(e.b))continue;
            const auto a=airport.nodes.at(e.a),b=airport.nodes.at(e.b);
            const double length=oans::length(b-a),score=oans::length((a+b)*.5-arp);
            if(length>=distance+20&&score<best) {
                best=score;edge_length=length;route=e.name;
                const auto u=(b-a)*(1/length),mid=(a+b)*.5;
                from=mid-u*(distance/2);to=mid+u*(distance/2);
            }
        }
        if(route.empty())throw std::runtime_error("No named taxi edge long enough for 30s at 15kt");
        const auto unit=(to-from)*(1/distance);
        const double heading=std::fmod(std::atan2(unit.x,unit.y)*180/oans::pi+360,360);
        auto geo=[&](oans::Vec2 p) {
            return oans::Geo{airport.lat+p.y/oans::earth*180/oans::pi,
                airport.lon+p.x/(oans::earth*std::cos(airport.lat*oans::pi/180))*180/oans::pi};
        };
        const auto start=geo(from),end=geo(to);
        std::cout<<std::setprecision(12)<<"apt="<<entry->file.u8string()<<"\n"
            <<"buildings="<<argv[3]<<"\nroute="<<route<<" edge_m="<<edge_length
            <<" heading_deg="<<heading<<" distance_m="<<distance<<"\n"
            <<"start_lat="<<start.lat<<" start_lon="<<start.lon<<" end_lat="<<end.lat<<" end_lon="<<end.lon<<"\n"
            <<"mode=ARC sides=1 seconds=30 hz=60 frames=1800 repeats="<<repeats
            <<" CPU_affinity=logical_0 no_GL no_simulator no_traffic no_route_overlay\n"<<std::flush;
        const auto load_start=std::chrono::steady_clock::now();
        oans::A350Map map(std::filesystem::u8path(argv[1]),*entry,std::filesystem::u8path(argv[3]));
        const auto stats=map.stats();
        std::cout<<"load_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-load_start).count()
            <<" pavements="<<stats.pavements<<" stand_labels="<<stats.stand_labels
            <<" terminals="<<stats.terminals<<" buildings="<<stats.buildings<<"\n"<<std::flush;
        std::ofstream out(std::filesystem::u8path(argv[4]));out.exceptions(std::ios::failbit|std::ios::badbit);
        out<<"round,quality,range_nm,frames,total_ms,mean_frame_ms,p50_frame_ms,p95_frame_ms,base_builds,base_hits,base_build_total_ms,mean_base_build_ms,mean_base_vertices,peak_base_vertices,mean_dynamic_vertices\n";
        const char* names[]={"ultra","max","xhigh","high","medium","low"};
        for(int round=0;round<repeats;++round)for(double range:{2.,1.,.2,.1})for(int slot=0;slot<6;++slot) {
            // Rotate/reverse order to reduce temperature and background-load bias.
            const int quality=round%2?(round+5-slot)%6:(round+slot)%6;
            oans::A350View view;view.mode=3;view.range=range;view.detail_quality=quality;
            view.psi=view.mag=heading;view.angle=heading*oans::pi/180;
            const auto vp=oans::map_viewport(3,false);
            view.anchor=vp.anchor;view.radius=vp.radius;view.bottom=vp.bottom;
            view.scale=vp.radius/(range*1852);
            auto pose=[&](int frame) {
                const auto position=geo(from+unit*(speed*frame/hz));
                view.lat=position.lat;view.lon=position.lon;
                view.center=oans::local(view.lat,view.lon,map.ref_lat(),map.ref_lon());
            };
            // Warm the same moving path, then force the initial map back to its start.
            for(int frame=0;frame<120;++frame) {
                pose(frame);map.render_base(0,view,frame/double(hz),false,frame);map.render_dynamic(view);
            }
            pose(0);map.render_base(0,view,0,true,0);map.render_dynamic(view);
            const auto before=map.cache_stats(0);
            std::vector<double> times;times.reserve(frames);
            std::uint64_t base_vertices=0,dynamic_vertices=0,peak_vertices=0;
            for(int frame=1;frame<=frames;++frame) {
                pose(frame);
                const auto t0=std::chrono::steady_clock::now();
                const auto base_count=map.render_base(0,view,frame/double(hz),false,frame).size();
                const auto dynamic_count=map.render_dynamic(view).size();
                const auto t1=std::chrono::steady_clock::now();
                times.push_back(std::chrono::duration<double,std::milli>(t1-t0).count());
                base_vertices+=base_count;dynamic_vertices+=dynamic_count;
                peak_vertices=std::max(peak_vertices,std::uint64_t(base_count));
            }
            const auto after=map.cache_stats(0);
            const auto builds=after.builds-before.builds,hits=after.hits-before.hits;
            const double total=std::accumulate(times.begin(),times.end(),0.),build_ms=after.total_ms-before.total_ms;
            std::sort(times.begin(),times.end());
            out<<std::fixed<<std::setprecision(8)<<round+1<<','<<names[quality]<<','<<range<<','<<frames<<','
                <<total<<','<<total/frames<<','<<times[frames/2]<<','<<times[(frames-1)*95/100]<<','
                <<builds<<','<<hits<<','<<build_ms<<','<<(builds?build_ms/builds:0)<<','
                <<double(base_vertices)/frames<<','<<peak_vertices<<','<<double(dynamic_vertices)/frames<<'\n'<<std::flush;
            std::cout<<"round="<<round+1<<" range="<<range<<" quality="<<names[quality]
                <<" mean_ms="<<total/frames<<" builds="<<builds<<" mean_vertices="<<base_vertices/frames<<'\n'<<std::flush;
        }
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
