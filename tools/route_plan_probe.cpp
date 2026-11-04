#include "route_plan.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
int main(int argc,char** argv) {
    if(argc<4){std::cerr<<"Usage: route_plan_probe apt.dat ICAO \"route\"\n";return 2;}
    std::vector<oans::IndexEntry> index;
    const auto source=std::filesystem::u8path(argv[1]);
    if(std::filesystem::is_directory(source)) {
        for(const auto& file:oans::scenery_files(source)) {
            auto entries=oans::scan_index(file);index.insert(index.end(),entries.begin(),entries.end());
        }
        index=oans::canonical_index(index);
    } else index=oans::scan_index(source);
    auto entry=oans::find_airport(index,argv[2]);
    if(!entry){std::cerr<<"Airport not found\n";return 2;}
    auto airport=oans::load_airport(*entry);
    auto route=oans::make_route_plan(airport,argv[3]);
    std::cout<<"Source: "<<entry->file.u8string()<<"\nStands: "<<route.start_stand<<" -> "<<route.end_stand<<'\n';
    if(argc>4) {
        std::ofstream f(std::filesystem::u8path(argv[4]));f<<std::setprecision(12);
        auto point=[&](oans::Vec2 p){f<<'['<<p.x<<','<<p.y<<']';};
        f<<"{\"icao\":"<<std::quoted(airport.icao)<<",\"input\":"<<std::quoted(argv[3])
         <<",\"source\":"<<std::quoted(entry->file.u8string())<<",\"error\":"<<std::quoted(route.error)
         <<",\"start_stand\":"<<std::quoted(route.start_stand)<<",\"end_stand\":"<<std::quoted(route.end_stand)
         <<",\"metres\":"<<route.metres<<",\"points\":[";
        bool comma=false;for(auto p:route.points){if(comma)f<<',';point(p);comma=true;}
        f<<"],\"runways\":[";comma=false;
        for(size_t i=0;i<airport.runways.size();i+=2){const auto& r=airport.runways[i];if(comma)f<<',';
            f<<"{\"name\":"<<std::quoted(r.name+"/"+r.reciprocal)<<",\"width\":"<<r.width<<",\"a\":";point(r.start);f<<",\"b\":";point(r.end);f<<'}';comma=true;}
        f<<"],\"stands\":[";comma=false;
        for(const auto& s:airport.stands){if(comma)f<<',';f<<"{\"name\":"<<std::quoted(s.name)<<",\"pos\":";point(s.pos);f<<'}';comma=true;}
        f<<"],\"edges\":[";comma=false;
        for(const auto& e:airport.edges){if(!airport.nodes.count(e.a)||!airport.nodes.count(e.b))continue;if(comma)f<<',';
            f<<"{\"name\":"<<std::quoted(e.name)<<",\"runway\":"<<e.runway<<",\"oneway\":"<<e.oneway<<",\"a\":";point(airport.nodes.at(e.a));f<<",\"b\":";point(airport.nodes.at(e.b));f<<'}';comma=true;}
        f<<"],\"pavements\":[";comma=false;
        for(const auto& p:airport.pavements){if(comma)f<<',';f<<'[';bool ringcomma=false;
            for(const auto& ring:p.rings){if(ringcomma)f<<',';f<<'[';bool pc=false;for(auto v:ring){if(pc)f<<',';point(v);pc=true;}f<<']';ringcomma=true;}f<<']';comma=true;}
        f<<"],\"ribbon\":[";comma=false;
        for(const auto& quad:oans::route_ribbon(route.points)){if(comma)f<<',';f<<'[';for(size_t i=0;i<4;++i){if(i)f<<',';point(quad[i]);}f<<']';comma=true;}
        f<<"]}";
    }
    if(!route){std::cerr<<route.error<<'\n';return 1;}
    std::cout<<airport.icao<<" / "<<route.metres<<" m / "<<route.points.size()<<" points\n";
    for(auto p:route.points)std::cout<<p.x<<','<<p.y<<'\n';
}
