#include "core.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>

int main(int argc,char** argv) {
    if(argc<3) {std::cerr<<"Usage: apt_inspect XPLANE_ROOT ICAO [output.svg]\n";return 2;}
    try {
        std::vector<oans::IndexEntry> index;size_t files=0;
        for(const auto& p:oans::scenery_files(std::filesystem::u8path(argv[1]))) {
            auto entries=oans::scan_index(p);index.insert(index.end(),entries.begin(),entries.end());++files;
        }
        std::cout<<"files="<<files<<" index_entries="<<index.size()<<'\n';
        auto selected=oans::find_airport(index,argv[2]);
        if(selected) {
            const auto& e=*selected;
            auto a=oans::load_airport(e);
            std::cout<<"icao="<<a.icao<<" header_id="<<a.header_id<<" id_source="<<(a.icao_explicit?"1302 icao_code":"header fallback (not confirmed ICAO)")<<" name="<<a.name<<"\nsource="<<e.file.u8string()<<"\n"
                <<"runway_directions="<<a.runways.size()<<" pavement_polygons="<<a.pavements.size()<<" stands="<<a.stands.size()<<" route_nodes="<<a.nodes.size()<<" route_edges="<<a.edges.size()<<" painted_segments="<<a.markings.size()<<'\n';
            std::map<int,size_t> styles;for(const auto& m:a.markings)++styles[m.style];
            for(auto [style,count]:styles)std::cout<<"paint_style="<<style<<" segments="<<count<<'\n';
            for(const auto& r:a.runways)std::cout<<"RWY "<<r.name<<" length_m="<<r.length()<<" displacement_m="<<r.displaced<<'\n';
            if(argc>3) {
                double minx=0,maxx=0,miny=0,maxy=0;
                for(const auto& r:a.runways)for(auto p:{r.start,r.end}) {minx=std::min(minx,p.x);maxx=std::max(maxx,p.x);miny=std::min(miny,p.y);maxy=std::max(maxy,p.y);}
                std::ofstream f(std::filesystem::u8path(argv[3]));double w=maxx-minx+1200,h=maxy-miny+1200;
                f<<std::setprecision(10)<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1000\" height=\"800\" viewBox=\""<<minx-600<<' '<<-maxy-600<<' '<<w<<' '<<h<<"\">\n"
                 <<"<rect x=\""<<minx-600<<"\" y=\""<<-maxy-600<<"\" width=\""<<w<<"\" height=\""<<h<<"\" fill=\"#08111b\"/>\n";
                for(const auto& poly:a.pavements) {
                    f<<"<path fill=\"#384149\" fill-rule=\"evenodd\" d=\"";
                    for(const auto& ring:poly.rings) {bool first=true;for(auto p:ring) {f<<(first?'M':'L')<<p.x<<','<<-p.y<<' ';first=false;}f<<"Z ";}f<<"\"/>\n";
                }
                for(size_t i=0;i<a.runways.size();i+=2) {const auto& r=a.runways[i];
                    f<<"<path stroke=\"#bec9cc\" stroke-width=\""<<r.width<<"\" d=\"M"<<r.start.x<<','<<-r.start.y<<'L'<<r.end.x<<','<<-r.end.y<<"\"/>\n";
                }
                f<<"</svg>\n";
            }
            return 0;
        }
        std::cerr<<"Airport not found\n";return 1;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
