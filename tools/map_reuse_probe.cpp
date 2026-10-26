#include "a350_map.hpp"
#include "map_viewport.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <type_traits>

namespace {bool counting=false;size_t allocations=0;}
// Count C++ heap allocations only. Lua's own allocator is deliberately outside
// this metric; the upstream drawing pipeline still creates temporary Lua data.
void* operator new(std::size_t size) {
    if(counting)++allocations;
    if(void* p=std::malloc(size?size:1))return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}

int main(int argc,char** argv) {
    try {
        if(argc<5)throw std::runtime_error("Usage: map_reuse_probe ASSETS XPLANE_ROOT ICAO BUILDINGS [--require-reuse]");
        const bool require_reuse=argc>5;
        if(require_reuse&&(!std::is_lvalue_reference_v<decltype(std::declval<oans::A350Map>().render_dynamic({}))>||
           !std::is_lvalue_reference_v<decltype(std::declval<oans::A350Map>().cursor({}))>))
            throw std::runtime_error("Borrowed output API required");
        std::vector<oans::IndexEntry> index;
        for(const auto& file:oans::scenery_files(std::filesystem::u8path(argv[2]))) {
            auto entries=oans::scan_index(file);index.insert(index.end(),entries.begin(),entries.end());
        }
        auto entry=oans::find_airport(oans::canonical_index(index),argv[3]);if(!entry)throw std::runtime_error("Airport absent");
        oans::A350Map map(std::filesystem::u8path(argv[1]),*entry,std::filesystem::u8path(argv[4]));
        oans::A350View view;view.mode=3;view.range=.5;view.lat=map.ref_lat();view.lon=map.ref_lon();
        auto vp=oans::map_viewport(3,false);view.anchor=vp.anchor;view.radius=vp.radius;view.bottom=vp.bottom;view.scale=vp.radius/(.5*1852);
        view.crosses={{20,30}};view.flags={{40,50}};view.traffic={{1,view.lat,view.lon,90}};
        view.runway_ahead={{"02L","20R",2,50}};view.alert_blink=true;
        const auto other=view;
        const auto saved_base=map.render_base(0,view,0);
        std::cout<<"CPU only; C++ allocations exclude Lua malloc; frames=1000 after warmup\nscenario,mean_ms,cpp_allocations,consumed_vertices\n";
        auto measure=[&](const char* name,auto fn) {
            for(int i=0;i<10;++i)fn(i);
            allocations=0;counting=true;const auto start=std::chrono::steady_clock::now();size_t count=0;
            for(int i=0;i<1000;++i)count+=fn(i);
            const auto stop=std::chrono::steady_clock::now();counting=false;
            const auto n=allocations;
            std::cout<<name<<','<<std::chrono::duration<double,std::milli>(stop-start).count()/1000<<','<<n<<','<<count<<'\n';
            if(require_reuse&&n)throw std::runtime_error(std::string(name)+" allocated C++ heap after warmup");
        };
        measure("content_compare",[&](int){return size_t(view.same_map_content(other));});
        measure("dynamic",[&](int i){view.psi=i*.01;return map.render_dynamic(view).size();});
        measure("cursor",[&](int i){return map.cursor({300.+i%2,300}).size();});
        measure("plan_notice",[&](int i){return map.plan_notice(i%2?4:3,true).size();});
        measure("advisory",[&](int i){return map.advisory(3,false,view.runway_ahead,"02L",i%2).size();});
        const auto& retained=map.render_base(0,view,.01);
        if(retained.size()!=saved_base.size())throw std::runtime_error("Dynamic/UI damaged base buffer");
        for(size_t i=0;i<retained.size();++i) {
            const auto& a=retained[i];const auto& b=saved_base[i];
            if(a.x!=b.x||a.y!=b.y||a.u!=b.u||a.v!=b.v||a.rgba!=b.rgba)throw std::runtime_error("Retained base data changed");
        }
        if(require_reuse) {
            const auto& cursor=map.cursor({300,300});const auto* cursor_data=cursor.data();const auto cursor_copy=cursor;
            const auto& notice=map.plan_notice(4,true);const auto* notice_data=notice.data();
            const auto& advisory=map.advisory(3,false,view.runway_ahead,"02L",true);const auto* advisory_data=advisory.data();
            map.render_dynamic(view);map.plan_notice(3,false);map.advisory(3,false,{},"",false);
            if(cursor.data()!=cursor_data||cursor.size()!=cursor_copy.size()||notice.data()!=notice_data||advisory.data()!=advisory_data)
                throw std::runtime_error("Independent UI output lifetime broken");
            map.cursor({301,300});
            if(map.cursor({300,300}).data()!=cursor_data)throw std::runtime_error("Cursor capacity not reused");
        }
        std::cout<<"PASS base isolation, independent UI lifetime and warmed C++ allocation checks\n";
    }catch(const std::exception& e){counting=false;std::cerr<<e.what()<<'\n';return 1;}
}
