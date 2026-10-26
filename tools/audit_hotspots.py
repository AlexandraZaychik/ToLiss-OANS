"""Offline diagnostic only: profile copied map source/assets; never build/install a plugin.

Run with E:\\conda_env\\normal\\python.exe -X utf8 tools/audit_hotspots.py
Uses the existing compiler, Lua archive, and local X-Plane airport data.
"""
import argparse
import pathlib
import shutil
import subprocess


HEADER = r'''
#pragma once
#include <chrono>
#include <map>
#include <string>
#include <iostream>
#include <iomanip>
extern "C" {
#include "lua.h"
}
namespace audit {
using Clock=std::chrono::steady_clock;
inline bool enabled=false;
inline std::map<std::string,double> totals;
inline void record(const char* name, Clock::time_point start) {
    if(enabled)totals[name]+=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
}
inline int now(lua_State* L) {
    lua_pushnumber(L,std::chrono::duration<double>(Clock::now().time_since_epoch()).count());return 1;
}
inline int add(lua_State* L) {
    if(enabled)totals[lua_tostring(L,1)]+=lua_tonumber(L,2)*1000;return 0;
}
inline void bind(lua_State* L) {
    lua_pushcfunction(L,now);lua_setglobal(L,"audit_now");
    lua_pushcfunction(L,add);lua_setglobal(L,"audit_add");
}
inline void dump(int frames) {
    for(const auto& p:totals)std::cout<<p.first<<','<<std::fixed<<std::setprecision(6)<<p.second/frames<<'\n';
}
}
'''

LUA = r'''
-- Diagnostic wrappers only, appended to a private copy of the generated asset.
local function audit_wrap(tbl,key,label)
    local original=tbl[key]
    tbl[key]=function(...)
        local t=audit_now()
        local a,b,c=original(...)
        audit_add(label,audit_now()-t)
        return a,b,c
    end
end
for _,key in ipairs({'setup','surfaces','runways','labels','draw_tower'}) do
    audit_wrap(MAPX,key,'lua_'..key)
end
audit_wrap(NDUI,'compass','lua_compass')
local audit_surface_steps=MAPX.surface_steps
MAPX.surface_steps=function(C)
    local steps=audit_surface_steps(C)
    for i,f in ipairs(steps) do
        steps[i]=function(...)
            local t=audit_now()
            local result=f(...)
            audit_add('lua_surface_step_'..i,audit_now()-t)
            return result
        end
    end
    return steps
end
'''

PROBE = r'''
#include "a350_map.hpp"
#include "map_viewport.hpp"
#include "nd_ui.hpp"
#include "hotspot_instrumentation.hpp"
#include <windows.h>
#include <algorithm>
#include <stdexcept>
volatile std::size_t consumed=0;
int main(int argc,char** argv) {
 try {
    if(argc!=7)throw std::runtime_error("ASSETS AUDITED_ASSETS XPLANE ICAO BUILDINGS FRAMES required");
    SetProcessAffinityMask(GetCurrentProcess(),1);
    const int frames=std::stoi(argv[6]);
    std::vector<oans::IndexEntry> index;
    for(const auto& file:oans::scenery_files(std::filesystem::u8path(argv[3]))) {
        auto rows=oans::scan_index(file);index.insert(index.end(),rows.begin(),rows.end());
    }
    index=oans::canonical_index(index);
    index=oans::filter_airport_index(index,oans::minimum_runway_length("A20N"),oans::minimum_runway_width("A20N"));
    auto entry=oans::find_airport(index,argv[4]);if(!entry)throw std::runtime_error("Airport missing");
    std::cout<<"CPU only; Release O2; CPU affinity=1; airport="<<argv[4]<<" index_size="<<index.size()<<" frames="<<frames<<"\n";
    oans::A350Map reference(std::filesystem::u8path(argv[1]),*entry,std::filesystem::u8path(argv[5]));
    oans::A350Map map(std::filesystem::u8path(argv[2]),*entry,std::filesystem::u8path(argv[5]));
    oans::A350View base;base.mode=3;base.range=.5;
    base.lat=entry->icao=="ZUCK"?29.716054916:map.ref_lat();base.lon=entry->icao=="ZUCK"?106.651611328:map.ref_lon();
    base.psi=base.mag=250.77468872;base.angle=base.psi*oans::pi/180;
    auto vp=oans::map_viewport(3,false);base.anchor=vp.anchor;base.radius=vp.radius;base.bottom=vp.bottom;
    base.scale=vp.radius/(base.range*1852);base.center=oans::local(base.lat,base.lon,map.ref_lat(),map.ref_lon());
    auto same=[](const auto& a,const auto& b) {
        if(a.size()!=b.size())throw std::runtime_error("Instrumentation changed vertex count");
        for(size_t i=0;i<a.size();++i)if(a[i].x!=b[i].x||a[i].y!=b[i].y||a[i].u!=b[i].u||a[i].v!=b[i].v||a[i].rgba!=b[i].rgba)
            throw std::runtime_error("Instrumentation changed vertex fields");
    };
    for(double range:{.2,.5,1.})for(int layer:{1,2}) {
        auto view=base;view.range=range;view.scale=vp.radius/(range*1852);
        if(layer==1)same(reference.render_base(0,view,0,true),map.render_base(0,view,0,true));
        else same(reference.render_dynamic(view),map.render_dynamic(view));
        auto draw=[&](int i) {
            auto v=view;v.center.y+=2.*(i+1)/view.scale;
            v.lat+=(v.center.y-view.center.y)/oans::earth*180/oans::pi;
            v.psi+=.05*(i+1);v.mag=v.psi;v.angle=v.psi*oans::pi/180;
            return layer==1?map.render_base(0,v,(i+1)/60.,true).size():map.render_dynamic(v).size();
        };
        for(int i=0;i<20;++i)consumed+=draw(i);
        audit::totals.clear();audit::enabled=true;
        auto start=audit::Clock::now();
        for(int i=0;i<frames;++i)consumed+=draw(i);
        audit::record("total",start);audit::enabled=false;
        std::cout<<"section="<<(layer==1?"forced_base_refresh":"dynamic")<<" range="<<range<<"\nstage,mean_ms\n";
        audit::dump(frames);
    }
    std::cout<<"PASS instrumentation vertex equality at 3 ranges for base and dynamic\n";
    std::cout<<"section=ui_cpu; valid FMS codes ZBAA/ZUCK/ZSPD; no SDK or GL\nscenario,mean_ms\n";
    auto measure=[&](const char* name,auto f) {
        for(int i=0;i<20;++i)f();
        auto start=audit::Clock::now();for(int i=0;i<2000;++i)f();
        std::cout<<name<<','<<std::chrono::duration<double,std::milli>(audit::Clock::now()-start).count()/2000<<'\n';
    };
    oans::UiModel model;model.index_ready=true;model.profile=true;model.code=entry->icao;model.name=entry->name;
    auto lookup=[&] {
        if(oans::find_airport(index,"ZBAA"))model.fms_origin="ZBAA";
        if(oans::find_airport(index,"ZUCK"))model.fms_destination="ZUCK";
        if(oans::find_airport(index,"ZSPD"))model.fms_alternate="ZSPD";
        consumed+=model.fms_origin.size()+model.fms_destination.size()+model.fms_alternate.size();
    };
    measure("three_find_airport_calls",lookup);
    oans::UiState state;
    measure("closed_controls_only",[&]{consumed+=oans::nd_controls(state,model).controls.size();});
    measure("closed_controls_plus_fms_lookups",[&]{lookup();consumed+=oans::nd_controls(state,model).controls.size();});
    state.panel=oans::Panel::airport;
    measure("airport_controls_only",[&]{consumed+=oans::nd_controls(state,model).controls.size();});
    // Diagnostic counterfactual: index has already passed canonical_index(),
    // so duplicate selection can be omitted. Do not apply this to raw indexes.
    auto nearest_canonical=[&](double lat,double lon) {
        std::optional<oans::IndexEntry> best;double distance=oans::airport_search_radius;
        for(const auto& e:index) {
            double d=oans::length(oans::local(e.lat,e.lon,lat,lon));
            if(d<distance){distance=d;best=e;}
        }
        return best;
    };
    for(const auto& location:std::vector<oans::Geo>{{base.lat,base.lon},{40.08,116.58},{31.14,121.81},{0,0},{-33.95,151.18}}) {
        auto a=oans::nearest(index,location.lat,location.lon),b=nearest_canonical(location.lat,location.lon);
        if(bool(a)!=bool(b)||(a&&(a->icao!=b->icao||a->file!=b->file||a->offset!=b->offset)))
            throw std::runtime_error("Canonical nearest mismatch");
    }
    measure("nearest_airport",[&]{auto e=oans::nearest(index,base.lat,base.lon);consumed+=bool(e);});
    measure("nearest_already_canonical_counterfactual",[&]{auto e=nearest_canonical(base.lat,base.lon);consumed+=bool(e);});
    std::cout<<"PASS canonical-only nearest comparison at 5 locations (including no-match)\n";
    std::cout<<"consumed="<<consumed<<'\n';
 } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default=r'D:\Qt\Qt6\Tools\mingw1310_64\bin\g++.exe')
    parser.add_argument('--xplane', default=r'G:\SteamLibrary\steamapps\common\X-Plane 12')
    parser.add_argument('--icao', default='ZUCK')
    parser.add_argument('--frames', type=int, default=180)
    args = parser.parse_args()
    if args.frames < 2:
        parser.error('--frames must be >= 2')
    root = pathlib.Path(__file__).resolve().parents[1]
    work = root / 'build/hotspot-audit'
    work.mkdir(parents=True, exist_ok=True)
    for name, text in [('hotspot_instrumentation.hpp', HEADER), ('probe.cpp', PROBE)]:
        (work / name).write_text(text, encoding='utf-8')
    source = (root / 'src/a350_map.cpp').read_text(encoding='utf-8')
    def replace_once(old, new):
        nonlocal source
        if source.count(old) != 1:
            raise RuntimeError(f'Source changed; expected unique anchor: {old}')
        source = source.replace(old, new)
    replace_once('#include "lua_vertex_buffer.hpp"', '#include "lua_vertex_buffer.hpp"\n#include "hotspot_instrumentation.hpp"')
    replace_once('    p.draw_buffer.bind(L);', '    p.draw_buffer.bind(L);audit::bind(L);')
    replace_once('p.check(lua_pcall(L,26,2,0));', 'audit::record("cpp_prepare",started);auto lua_started=audit::Clock::now();p.check(lua_pcall(L,26,2,0));audit::record("lua_total",lua_started);')
    replace_once('    p.draw_buffer.copy(L,cache.vertices);', '    auto copy_started=audit::Clock::now();p.draw_buffer.copy(L,cache.vertices);audit::record("cpp_vertex_copy",copy_started);auto labels_started=audit::Clock::now();')
    replace_once('    ++cache.stats.builds;', '    audit::record("cpp_labels_cache",labels_started);\n    ++cache.stats.builds;')
    (work / 'a350_map.cpp').write_text(source, encoding='utf-8')
    assets = work / 'assets'
    shutil.copytree(root / 'assets', assets, dirs_exist_ok=True)
    engine = assets / 'a350/map_engine.lua'
    with engine.open('a', encoding='utf-8') as f:
        f.write(LUA)
    executable = work / 'probe.exe'
    command = [args.compiler, '-std=c++17', '-O2', '-DOANS_DEBUG=0', '-DNOMINMAX', '-static',
               '-Isrc', '-Ivendor/lua-5.4.9/src', '-I' + str(work), 'src/core.cpp',
               'src/runway_ahead.cpp', 'src/nd_ui.cpp', str(work / 'a350_map.cpp'),
               str(work / 'probe.cpp'), 'build/lua/liblua.a', '-o', str(executable)]
    subprocess.run(command, cwd=root, check=True)
    buildings = root / f'cache/dsf_buildings_{args.icao}.json'
    if not buildings.is_file():
        buildings = root / f'cache/osm2_{args.icao}.json'
    run = subprocess.run([str(executable), str(root / 'assets'), str(assets), args.xplane,
                          args.icao, str(buildings), str(args.frames)], cwd=root,
                         capture_output=True, text=True, encoding='utf-8', check=True)
    (work / 'results.txt').write_text(run.stdout, encoding='utf-8')
    print(run.stdout)


if __name__ == '__main__':
    main()
