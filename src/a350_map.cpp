#include "a350_map.hpp"
#include "map_reprojection.hpp"
#include "detail_log.hpp"
#include "lua_vertex_buffer.hpp"
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#include <cmath>
#include <stdexcept>
#include <chrono>

namespace oans {
std::vector<A350Traffic> filter_tcas_traffic(const std::vector<TcasTrafficSample>& samples,
                                             double own_lat,double own_lon) {
    std::vector<A350Traffic> result;
    if(!std::isfinite(own_lat)||!std::isfinite(own_lon)||std::abs(own_lat)>90||std::abs(own_lon)>180)return result;
    constexpr double max_distance=1852.0,max_speed=80.0*kt;
    for(const auto& s:samples) {
        if(s.slot<=0||s.mode_s_id==0||!std::isfinite(s.lat)||!std::isfinite(s.lon)||
           !std::isfinite(s.psi)||!std::isfinite(s.speed_mps)||s.speed_mps<0||s.speed_mps>=max_speed||
           std::abs(s.lat)>90||std::abs(s.lon)>180)continue;
        if(length(local(s.lat,s.lon,own_lat,own_lon))>max_distance)continue;
        result.push_back({s.slot,s.lat,s.lon,s.psi,s.flight_id,s.icao_type,
            std::isfinite(s.ground_speed_mps)&&s.ground_speed_mps>=0?int(std::lround(s.ground_speed_mps/kt)):-1});
    }
    return result;
}
bool A350View::reusable_with(const A350View& o) const {
    if(position_valid!=o.position_valid||!same_map_content(o))return false;
    if(traffic.size()!=o.traffic.size())return false;
    for(size_t i=0;i<traffic.size();++i) {
        const auto& a=traffic[i];const auto& b=o.traffic[i];
        if(a.slot!=b.slot||a.flight_id!=b.flight_id||a.icao_type!=b.icao_type||a.ground_speed_kt!=b.ground_speed_kt)return false;
        if(length(local(a.lat,a.lon,b.lat,b.lon))*scale+
           std::abs(std::remainder(a.psi-b.psi,360.0))*pi/180*16>=.05)return false;
    }
    // Compare motion to the last built frame in display pixels.
    constexpr double error_px=.05;
    const double rotation=std::abs(std::remainder(angle-o.angle,2*pi))*1000;
    const double translation=length(center-o.center)*scale;
    const double dx=std::remainder(lon-o.lon,360)*pi/180*earth*std::cos(lat*pi/180);
    const double dy=(lat-o.lat)*pi/180*earth;
    const double ownship=std::hypot(dx,dy)*scale;
    const double plane_turn=std::abs(std::remainder(psi-o.psi,360))*pi/180*40;
    const double compass_turn=std::abs(std::remainder(mag-o.mag,360))*pi/180*600;
    return translation+rotation<error_px&&ownship+rotation+plane_turn<error_px&&compass_turn<error_px;
}
bool A350View::same_map_content(const A350View& o) const {
    if(simple_stand_names!=o.simple_stand_names)return false;
    if(runway_ahead.size()!=o.runway_ahead.size())return false;
    if(!runway_ahead.empty()&&alert_blink!=o.alert_blink)return false;
    for(size_t i=0;i<runway_ahead.size();++i)
        if(runway_ahead[i].first!=o.runway_ahead[i].first||runway_ahead[i].second!=o.runway_ahead[i].second)return false;
    auto points=[](const auto& a,const auto& b) {
        if(a.size()!=b.size())return false;
        for(size_t i=0;i<a.size();++i)if(a[i].x!=b[i].x||a[i].y!=b[i].y)return false;
        return true;
    };
    if(anchor.x!=o.anchor.x||anchor.y!=o.anchor.y||scale!=o.scale||range!=o.range||mode!=o.mode||
       radius!=o.radius||bottom!=o.bottom||outside_plan!=o.outside_plan||fms_airport!=o.fms_airport||fms_runway!=o.fms_runway||map_runway!=o.map_runway||!points(crosses,o.crosses)||!points(flags,o.flags))return false;
    return true;
}
struct A350Map::Impl {
    lua_State* L=nullptr;
    LuaVertexBuffer draw_buffer;
    const std::atomic<bool>* cancel=nullptr;
    A350Stats stats;
    std::optional<Geo> tower;
    double lat=0,lon=0;
    struct Cache {bool ready=false,motion_deferred=false;int layer=0;double time=0,deferred_time=0;
        std::int64_t deferred_frame=-1;A350View view;std::vector<A350Vertex> vertices;A350CacheStats stats;std::vector<A350RunwayLabel> labels;};
    std::array<Cache,4> cache;
    double last_base_motion_time=-1;
    std::int64_t last_base_motion_frame=-1;
    std::array<A350Switch,2> switches;
    std::vector<A350Vertex> cursor_vertices,notice_vertices,advisory_vertices;
    const std::vector<A350Vertex> empty_vertices;
    bool notice_ready=false;
    ~Impl(){OANS_TRACE_SCOPE();if(L)lua_close(L);}
    static int cancelled(lua_State* L) {
        auto p=static_cast<Impl*>(lua_touserdata(L,lua_upvalueindex(1)));
        lua_pushboolean(L,p->cancel&&p->cancel->load());return 1;
    }
    void check(int result) {
        if(result!=LUA_OK) {
            std::string e=lua_tostring(L,-1)?lua_tostring(L,-1):"unknown Lua error";
            OANS_TRACE("Lua failure: "+e);lua_settop(L,0);throw std::runtime_error("A350 map: "+e);
        }
    }
    void string(const std::string& s){lua_pushlstring(L,s.data(),s.size());}
    void alerts(const std::vector<RunwayAheadHit>& hits) {
        lua_createtable(L,static_cast<int>(hits.size()),0);int i=0;
        for(const auto& hit:hits) {lua_createtable(L,2,0);string(hit.first);lua_rawseti(L,-2,1);
            string(hit.second);lua_rawseti(L,-2,2);lua_rawseti(L,-2,++i);}
    }
    void points(const std::vector<Vec2>& pts) {
        lua_createtable(L,static_cast<int>(pts.size()),0);
        int i=0;for(auto p:pts) {lua_createtable(L,2,0);lua_pushnumber(L,p.x);lua_rawseti(L,-2,1);
            lua_pushnumber(L,p.y);lua_rawseti(L,-2,2);lua_rawseti(L,-2,++i);}
    }
    void traffic(const std::vector<A350Traffic>& targets) {
        lua_createtable(L,static_cast<int>(targets.size()),0);
        int i=0;for(const auto& target:targets) {
            lua_createtable(L,6,0);lua_pushnumber(L,target.lat);lua_rawseti(L,-2,1);
            lua_pushnumber(L,target.lon);lua_rawseti(L,-2,2);lua_pushnumber(L,target.psi);lua_rawseti(L,-2,3);
            string(target.flight_id);lua_rawseti(L,-2,4);lua_pushinteger(L,target.ground_speed_kt);lua_rawseti(L,-2,5);
            string(target.icao_type);lua_rawseti(L,-2,6);
            lua_rawseti(L,-2,++i);
        }
    }
    double number(int table,int index) {
        lua_rawgeti(L,table,index);double v=lua_tonumber(L,-1);lua_pop(L,1);return v;
    }
};
A350Map::A350Map(const std::filesystem::path& assets):p_(std::make_unique<Impl>()) {
    OANS_TRACE_SCOPE();
    auto& p=*p_;p.L=luaL_newstate();
    if(!p.L)throw std::runtime_error("Cannot create private A350 map Lua state");
    auto L=p.L;
    p.draw_buffer.bind(L);
    // Only the libraries actually used by the extracted map; no OS execution,
    // dynamic libraries, debug hooks, simulator globals or aircraft writes.
    for(auto lib:std::array<std::pair<const char*,lua_CFunction>,6>{{
        {LUA_GNAME,luaopen_base},{LUA_TABLIBNAME,luaopen_table},{LUA_STRLIBNAME,luaopen_string},
        {LUA_MATHLIBNAME,luaopen_math},{LUA_COLIBNAME,luaopen_coroutine},{LUA_IOLIBNAME,luaopen_io}}}) {
        luaL_requiref(L,lib.first,lib.second,1);lua_pop(L,1);
    }
    // The upstream geometry has a package.config platform check, but no loader.
    lua_newtable(L);lua_pushliteral(L,LUA_DIRSEP "\n;\n?\n!\n-\n");lua_setfield(L,-2,"config");lua_setglobal(L,"package");
    lua_pushlightuserdata(L,&p);lua_pushcclosure(L,Impl::cancelled,1);lua_setglobal(L,"native_cancelled");
    OANS_TRACE("Lua map_engine load/execute assets="+assets.u8string());
    p.check(luaL_loadfile(L,(assets/"a350/map_engine.lua").u8string().c_str()));p.check(lua_pcall(L,0,0,0));
    OANS_TRACE("Lua a350_load_font begin");
    lua_getglobal(L,"a350_load_font");p.string((assets/"a350").u8string());p.check(lua_pcall(L,1,0,0));
    lua_getglobal(L,"a350_load_wtc");p.string((assets/"wtc.json").u8string());p.check(lua_pcall(L,1,0,0));
    for(int on=0;on<2;++on) {
        lua_getglobal(L,"a350_oans_switch");lua_pushboolean(L,on);p.check(lua_pcall(L,1,5,0));
        auto& button=p.switches[on];p.draw_buffer.copy(L,button.vertices);
        for(int i=0;i<4;++i)button.bounds[i]=lua_tonumber(L,2+i);
        lua_settop(L,0);
    }
}
A350Map::A350Map(const std::filesystem::path& assets,const IndexEntry& e,
                 const std::filesystem::path& buildings,const std::atomic<bool>* cancel):A350Map(assets) {
    OANS_TRACE_SCOPE();
    auto& p=*p_;p.cancel=cancel;auto L=p.L;
    OANS_TRACE("Lua a350_load airport="+e.icao+" apt="+e.file.u8string()+" buildings="+buildings.u8string());
    lua_getglobal(L,"a350_load");p.string(e.file.u8string());lua_pushinteger(L,e.offset);p.string(e.icao);
    p.string((assets/"a350").u8string());p.string(buildings.u8string());p.check(lua_pcall(L,5,9,0));
    p.lat=lua_tonumber(L,1);p.lon=lua_tonumber(L,2);
    p.stats={int(lua_tointeger(L,3)),int(lua_tointeger(L,4)),int(lua_tointeger(L,5)),int(lua_tointeger(L,6)),int(lua_tointeger(L,7))};
    if(lua_isnumber(L,8)&&lua_isnumber(L,9))p.tower=Geo{lua_tonumber(L,8),lua_tonumber(L,9)};
    lua_settop(L,0);
}
A350Map::~A350Map()=default;
const std::vector<A350Vertex>& A350Map::cursor(Vec2 at,bool horizontal,bool yellow) {
    OANS_TRACE_SCOPE();
    auto& p=*p_;auto L=p.L;lua_settop(L,0);lua_getglobal(L,"a350_cursor");
    lua_pushnumber(L,at.x);lua_pushnumber(L,at.y);
    lua_pushboolean(L,horizontal);lua_pushboolean(L,yellow);p.check(lua_pcall(L,4,1,0));
    p.draw_buffer.copy(L,p.cursor_vertices);lua_settop(L,0);return p.cursor_vertices;
}
const std::vector<A350Vertex>& A350Map::plan_notice(int mode,bool outside) {
    OANS_TRACE_SCOPE();
    auto& p=*p_;
    if(mode!=4||!outside)return p.empty_vertices;
    if(p.notice_ready)return p.notice_vertices;
    auto L=p.L;lua_settop(L,0);lua_getglobal(L,"a350_plan_notice");
    p.string(mode==4?"plan":"other");lua_pushboolean(L,outside);p.check(lua_pcall(L,2,1,0));
    p.draw_buffer.copy(L,p.notice_vertices);lua_settop(L,0);p.notice_ready=true;return p.notice_vertices;
}
const A350Switch& A350Map::oans_switch(bool on) const{return p_->switches[on?1:0];}
A350Stats A350Map::stats() const{return p_->stats;}
A350CacheStats A350Map::cache_stats(int side) const{return p_->cache.at(side).stats;}
const std::vector<A350RunwayLabel>& A350Map::runway_labels(int side) const{return p_->cache.at(side).labels;}
std::optional<Geo> A350Map::tower() const{return p_->tower;}
double A350Map::ref_lat() const{return p_->lat;}
double A350Map::ref_lon() const{return p_->lon;}
const std::vector<A350Vertex>& A350Map::render(int side,const A350View& view) {
    auto& cache=p_->cache.at(side);
    if(cache.ready&&cache.layer==0&&cache.view.reusable_with(view)){++cache.stats.hits;return cache.vertices;}
    return render_impl(side,view,0);
}
bool A350View::base_reusable_with(const A350View& o) const {
    return same_map_content(o)&&length(center-o.center)*scale+
        std::abs(std::remainder(angle-o.angle,2*pi))*std::max(600.,radius)<1.;
}
const std::vector<A350Vertex>& A350Map::render_base(int side,const A350View& view,double now,bool interactive,std::int64_t frame) {
    auto& cache=p_->cache.at(side);
    const bool motion_only=cache.ready&&cache.layer==1&&cache.view.same_map_content(view)&&
        now>=cache.time&&map_texture_covers(cache.view,view);
    if(motion_only) {
        const double age=now-cache.time;
        const bool same_frame=frame>=0?frame==p_->last_base_motion_frame:std::abs(now-p_->last_base_motion_time)<1e-6;
        const bool deferred_this_frame=frame>=0?frame==cache.deferred_frame:std::abs(now-cache.deferred_time)<1e-6;
        const bool stagger=same_frame&&(!cache.motion_deferred||deferred_this_frame);
        if(cache.view.base_reusable_with(view)||(!interactive&&(age<.1-1e-9||stagger))) {
            if(stagger&&age>=.1-1e-9&&!cache.motion_deferred) {
                cache.motion_deferred=true;cache.deferred_time=now;cache.deferred_frame=frame;
            }
            ++cache.stats.hits;return cache.vertices;
        }
    }
    const auto& result=render_impl(side,view,1);cache.time=now;cache.motion_deferred=false;
    if(motion_only&&!interactive){p_->last_base_motion_time=now;p_->last_base_motion_frame=frame;}
    return result;
}
const A350View& A350Map::base_view(int side) const{return p_->cache.at(side).view;}
const std::vector<A350Vertex>& A350Map::render_dynamic(const A350View& view) {
    // The tiny dynamic layer has its own buffer and never invalidates the base.
    return render_impl(3,view,2);
}
const std::vector<A350Vertex>& A350Map::render_impl(int side,const A350View& view,int layer) {
    OANS_TRACE_SCOPE();
    OANS_TRACE("A350 render side="+std::to_string(side)+" mode="+std::to_string(view.mode)+" range="+std::to_string(view.range)+" scale="+std::to_string(view.scale));
    if(side<0||side>=4)throw std::out_of_range("A350 map side");
    if(!std::isfinite(view.scale)||view.scale<=0||!std::isfinite(view.angle))throw std::invalid_argument("A350 view scale/angle");
    auto& p=*p_;auto& cache=p.cache[side];
    const auto started=std::chrono::steady_clock::now();
    auto L=p.L;lua_settop(L,0);lua_getglobal(L,"a350_render");
    for(double n:{view.center.x,view.center.y,view.scale,view.angle,view.anchor.x,view.anchor.y,
                  view.lat,view.lon,view.psi,view.mag})lua_pushnumber(L,n);
    p.string(view.mode==3?"arc":view.mode==2?"rose":"plan");lua_pushnumber(L,view.range);
    p.points(view.crosses);p.points(view.flags);
    lua_pushnumber(L,view.radius>0?view.radius:(view.mode==3?370:245));lua_pushnumber(L,view.bottom);
    p.string(view.fms_airport);p.string(view.fms_runway);p.alerts(view.runway_ahead);lua_pushboolean(L,view.alert_blink);
    lua_pushboolean(L,view.outside_plan);p.string(view.map_runway);p.traffic(view.traffic);
    lua_pushboolean(L,view.position_valid);lua_pushboolean(L,view.simple_stand_names);lua_pushinteger(L,layer);p.check(lua_pcall(L,26,2,0));
    OANS_TRACE("Lua a350_render complete; reading buffers");
    p.draw_buffer.copy(L,cache.vertices);
    cache.labels.clear();
    for(size_t i=1;i<=lua_rawlen(L,2);++i) {
        lua_rawgeti(L,2,i);const int row=lua_gettop(L);
        auto str=[&](int n){lua_rawgeti(L,row,n);std::string s=lua_tostring(L,-1);lua_pop(L,1);return s;};
        auto point=[&](int n){return Vec2{p.number(row,n)*2,600-p.number(row,n+1)*2};};
        cache.labels.push_back({str(1),str(2),str(3),point(4),point(6),point(8)});lua_pop(L,1);
    }
    lua_settop(L,0);cache.view=view;cache.ready=true;cache.layer=layer;
    ++cache.stats.builds;cache.stats.last_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    cache.stats.total_ms+=cache.stats.last_ms;return cache.vertices;
}
const std::vector<A350Vertex>& A350Map::advisory(int mode,bool pfd,const std::vector<RunwayAheadHit>& hits,
                                        const std::string& fms_runway,bool blink) {
    OANS_TRACE_SCOPE();
    if(hits.empty()||!blink)return p_->empty_vertices;
    auto& p=*p_;auto L=p.L;lua_settop(L,0);lua_getglobal(L,"a350_advisory");
    p.string(mode==3?"arc":mode==2?"rose":"plan");lua_pushboolean(L,pfd);
    p.alerts(hits);p.string(fms_runway);p.check(lua_pcall(L,4,1,0));
    p.draw_buffer.copy(L,p.advisory_vertices);lua_settop(L,0);return p.advisory_vertices;
}
}
