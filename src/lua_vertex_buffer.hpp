#pragma once
#include "a350_map.hpp"
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}
#include <cmath>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace oans {
// Lua keeps all upstream geometry arithmetic. Each call only expands a primitive
// into the original triangle order, directly in a contiguous native buffer.
class LuaVertexBuffer {
    size_t limit_=180000;
    std::vector<A350Vertex> data_;
    static LuaVertexBuffer& target(lua_State* L,int args,size_t count) {
        auto& self=*static_cast<LuaVertexBuffer*>(lua_touserdata(L,lua_upvalueindex(1)));
        luaL_argcheck(L,lua_gettop(L)==args,1,"invalid vertex batch arguments");
        luaL_argcheck(L,count==0||(count<=self.limit_&&self.data_.size()<=self.limit_-count),1,"vertex buffer capacity exceeded");
        // Ultra has no fixed geometry cap. Grow before emitting a complete batch;
        // report allocation failure through Lua without unwinding across pcall.
        bool allocated=true;
        if(self.data_.size()+count>self.data_.capacity()) {
            try {self.data_.reserve(std::max(self.data_.size()+count,
                std::min(self.limit_,self.data_.capacity()*2)));}
            catch(const std::exception&) {allocated=false;}
        }
        if(!allocated)luaL_error(L,"vertex buffer allocation failed");
        return self;
    }
    static float scalar(lua_State* L,int i) {
        const float value=static_cast<float>(luaL_checknumber(L,i));
        luaL_argcheck(L,std::isfinite(value),i,"nonfinite draw vertex");
        return value;
    }
    static std::uint32_t colour(lua_State* L,int i) {
        const auto value=luaL_checkinteger(L,i);
        luaL_argcheck(L,value>=0&&static_cast<lua_Unsigned>(value)<=0xffffffffu,i,"invalid packed colour");
        return static_cast<std::uint32_t>(value);
    }
    void vertex(float x,float y,float u,float v,std::uint32_t c){data_.push_back({x,y,u,v,c});}
    static int reset(lua_State* L) {target(L,0,0).data_.clear();return 0;}
    static int triangle(lua_State* L) {
        auto& self=target(L,9,3);
        float xy[6];for(int i=0;i<6;++i)xy[i]=scalar(L,i+1);
        const auto c=colour(L,7);const float u=scalar(L,8),v=scalar(L,9);
        for(int i=0;i<6;i+=2)self.vertex(xy[i],xy[i+1],u,v,c);
        return 0;
    }
    static int quad(lua_State* L) {
        auto& self=target(L,9,6);
        float a[8];for(int i=0;i<8;++i)a[i]=scalar(L,i+1);
        const auto c=colour(L,9);
        const float xy[]={a[0],a[1],a[2],a[1],a[2],a[3],a[0],a[1],a[2],a[3],a[0],a[3]};
        const float uv[]={a[4],a[5],a[6],a[5],a[6],a[7],a[4],a[5],a[6],a[7],a[4],a[7]};
        for(int i=0;i<12;i+=2)self.vertex(xy[i],xy[i+1],uv[i],uv[i+1],c);
        return 0;
    }
    static int quad4(lua_State* L) {
        auto& self=target(L,13,6);
        float a[12];for(int i=0;i<12;++i)a[i]=scalar(L,i+1);
        const auto c=colour(L,13);
        const int corners[]={0,2,4,0,4,6};
        const float uv[]={a[8],a[9],a[10],a[9],a[10],a[11],a[8],a[9],a[10],a[11],a[8],a[11]};
        for(int i=0;i<6;++i)self.vertex(a[corners[i]],a[corners[i]+1],uv[2*i],uv[2*i+1],c);
        return 0;
    }
    static int line(lua_State* L) {
        auto& self=target(L,20,18);
        float xy[16];for(int i=0;i<16;++i)xy[i]=scalar(L,i+1);
        const auto c=colour(L,17),transparent=colour(L,18);
        const float u=scalar(L,19),v=scalar(L,20);
        // p1,p2,p3,p4,q1,q2,r1,r2, with the exact upstream AA edge colours.
        const int corners[]={0,1,2,0,2,3,0,1,5,0,5,4,3,2,7,3,7,6};
        for(int i=0;i<18;++i) {
            const int k=corners[i];self.vertex(xy[2*k],xy[2*k+1],u,v,k<4?c:transparent);
        }
        return 0;
    }
public:
    LuaVertexBuffer(){data_.reserve(limit_);}
    void detail_quality(int quality) {
        limit_=quality==0?std::numeric_limits<size_t>::max():quality==1?270000:180000;
        if(quality==1)data_.reserve(limit_);
    }
    void bind(lua_State* L) {
        const std::pair<const char*,lua_CFunction> functions[]={
            {"native_draw_reset",reset},{"native_draw_tri",triangle},{"native_draw_quad",quad},
            {"native_draw_quad4",quad4},{"native_draw_line",line}};
        for(const auto& f:functions){lua_pushlightuserdata(L,this);lua_pushcclosure(L,f.second,1);lua_setglobal(L,f.first);}
    }
    void copy(lua_State* L,std::vector<A350Vertex>& out) const {
        int valid=0;const auto count=lua_tointegerx(L,1,&valid);
        if(!valid||count<0||count%3||static_cast<lua_Unsigned>(count)!=data_.size())
            throw std::runtime_error("A350 invalid triangle buffer");
        out.assign(data_.begin(),data_.end());
    }
};
}
