#include "a350_map.hpp"
#include "test_support.hpp"
#include "map_viewport.hpp"
#include "route_clip.hpp"
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
        const auto& off=map.oans_switch(false);const auto& half=map.oans_switch(true,true);const auto& full=map.oans_switch(true);
        verify(off.bounds==half.bounds&&half.bounds==full.bounds,"three switch glyphs share hit bounds");
        verify(half.vertices.size()>off.vertices.size()&&full.vertices.size()>off.vertices.size(),"half and full switches have visible state glyphs");
        verify(half.vertices.size()!=full.vertices.size(),"half dot differs from full check mark");
        A350View view;view.lat=30;view.lon=120.01;view.center=local(view.lat,view.lon,30,120);
        verify(view.detail_quality==4,"detail quality defaults to medium");
        auto detail_view=view;detail_view.detail_quality=2;
        verify(!view.same_map_content(detail_view)&&!view.reusable_with(detail_view),"quality changes invalidate cached map content");
        view.scale=.2;view.mode=4;view.traffic={{1,30.001,120.01,90,"TEST","A320",10}};
        // Exercise the real atlas quads, including the large "21" at 210 deg.
        // Checking the ring alone misses captions erased by the host header.
        view.mode=3;
        for(int heading:{210}) {
            scenario("map/arc-210-header-regression");
            view.mag=heading;
            const auto closed=map_viewport(3,false,110),open=map_viewport(3,true,110);
            view.anchor=closed.anchor;view.radius=closed.radius;view.bottom=closed.bottom;
            const auto before=map.render_compass(view);
            view.anchor=open.anchor;view.radius=open.radius;view.bottom=open.bottom;
            const auto after=map.render_compass(view);
            if(before.size()!=after.size())throw std::runtime_error("Menu changes compass glyph count");
            bool text=false;
            for(size_t i=0;i<after.size();++i) {
                const auto& a=before[i];const auto& b=after[i];
                if(a.x!=b.x||a.y!=b.y||a.u!=b.u||a.v!=b.v||a.rgba!=b.rgba)
                    throw std::runtime_error("Menu moves compass geometry");
                if(b.rgba==0xFFFFFFFF) {
                    text=true;
                    if(b.y>=552)throw std::runtime_error("Heading glyph is covered by the host header");
                }
            }
            if(!text)throw std::runtime_error("ARC lost its heading labels");
        }
        scenario("map/manual-runway-overrides-fms");
        const auto airport=load_airport(entry);const auto& runway=airport.runways.at(0);
        view.position_valid=false;view.mode=3;view.map_runway=runway.reciprocal;view.fms_airport=entry.icao;view.fms_runway=runway.name;
        map.render(0,view);bool selected=false;
        for(const auto& label:map.runway_labels(0))selected|=label.selected==runway.reciprocal;
        verify(selected,"manual reciprocal endpoint overrides conflicting FMS endpoint");
        const auto& warning=map.advisory(3,false,{{runway.name,runway.reciprocal,0,0}},runway.reciprocal,true);
        bool green=false;for(const auto& vertex:warning)green|=vertex.rgba==0xFF00FF00;
        verify(green,"manually selected warning endpoint is green");
        scenario("map/blink-preserves-static-cache");
        view.mode=4;view.anchor={300,285};view.radius=245;view.bottom=40;
        view.runway_ahead={{runway.name,runway.reciprocal,0,0}};
        view.alert_blink=false;
        const auto surfaces=map.render_base(0,view,10);
        const auto labels=map.base_labels(0);
        const auto builds=map.cache_stats(0).builds;
        verify(!surfaces.empty()&&!labels.empty(),"surfaces and labels have independent cached output");
        verify(map.render_alert_fill(view).empty(),"blink-off skips alert geometry");
        auto blinking=view;blinking.alert_blink=true;
        verify(view.same_map_content(blinking)&&!view.reusable_with(blinking),
               "blink preserves base content but invalidates legacy combined output");
        verify(!map.render_alert_fill(blinking).empty(),"blink-on emits runway fill");
        map.render_base(0,blinking,11);
        verify(map.cache_stats(0).builds==builds,"blink toggle does not rebuild static layers");
        verify(map.base_labels(0).size()==labels.size(),"dynamic fill preserves cached labels");

        scenario("route/viewport-clipping");
        RouteClipper clipper;MapProjection projection;projection.radius=245;
        auto world=[&](std::array<Vec2,4> q){for(auto& p:q)p=projection.from(p);return q;};
        const auto crossing=world({{{-100,280},{700,280},{700,290},{-100,290}}});
        const auto clipped=clipper.clip(crossing,projection);
        verify(clipped.size()>=4,"ribbon with all corners outside still crosses viewport");
        for(auto p:clipped)verify(p.x>=-1e-8&&p.x<=600+1e-8&&p.y>=40-1e-8&&p.y<=552+1e-8&&
            length(p-projection.anchor)<=projection.radius+1e-8,"route vertices stay inside viewport");
        verify(clipper.clip(world({{{600,600},{700,600},{700,610},{600,610}}}),projection).empty(),
               "fully invisible ribbon is rejected");
        projection.bottom=300;
        const auto menu_crossing=clipper.clip(world({{{200,295},{400,295},{400,305},{200,305}}}),projection);
        verify(!menu_crossing.empty(),"partly occluded ribbon survives");
        for(auto p:menu_crossing)verify(p.y>=300-1e-8,"menu bottom clips the ribbon width");
        std::cout<<"PASS representative ARC heading: atlas quads clear header; menu preserves every compass vertex\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
