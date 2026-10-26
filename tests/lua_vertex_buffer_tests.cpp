#include "lua_vertex_buffer.hpp"
extern "C" {
#include "lualib.h"
}
#include <iostream>

int main() {
    lua_State* L=luaL_newstate();if(!L)return 1;
    try {
        oans::LuaVertexBuffer buffer;buffer.bind(L);
        luaL_requiref(L,"_G",luaopen_base,1);lua_pop(L,1);
        auto run=[&](const char* script,bool success=true) {
            lua_settop(L,0);const bool ok=luaL_dostring(L,script)==LUA_OK;
            if(ok!=success)throw std::runtime_error(ok?"Expected Lua batch rejection":lua_tostring(L,-1));
        };
        auto copy=[&](int n) {lua_settop(L,0);lua_pushinteger(L,n);std::vector<oans::A350Vertex> out;buffer.copy(L,out);return out;};
        // High-bit colours, fractional coordinates, UV mapping and triangle order.
        run("native_draw_reset(); native_draw_tri(.25,-2,3,4,5,6,0xFE123456,.125,.75)");
        auto tri=copy(3);
        if(tri[0].x!=.25f||tri[1].y!=4||tri[2].rgba!=0xFE123456||tri[2].u!=.125f||tri[0].v!=.75f)
            throw std::runtime_error("Triangle payload corrupted");
        run("native_draw_reset(); native_draw_quad(1,2,3,4,.1,.2,.3,.4,0xFFFFFFFF)");
        auto quad=copy(6);
        if(quad[1].x!=3||quad[1].y!=2||quad[5].x!=1||quad[5].y!=4||quad[5].u!=.1f||quad[5].v!=.4f)
            throw std::runtime_error("Quad UV mapping corrupted");
        run("native_draw_reset(); native_draw_quad4(1,2,3,4,5,6,7,8,.1,.2,.3,.4,0xFFFFFFFF)");
        auto rotated=copy(6);
        if(rotated[3].x!=1||rotated[4].x!=5||rotated[5].x!=7||rotated[5].v!=.4f)
            throw std::runtime_error("Rotated quad corrupted");
        run("native_draw_reset(); native_draw_line(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0xFF123456,0x00123456,.25,.5)");
        auto line=copy(18);
        const int expected[]={0,1,2,0,2,3,0,1,5,0,5,4,3,2,7,3,7,6};
        for(int i=0;i<18;++i)if(line[i].x!=2*expected[i]||line[i].y!=2*expected[i]+1||
            line[i].rgba!=(expected[i]<4?0xFF123456u:0x00123456u))throw std::runtime_error("AA topology/colour corrupted");
        run("native_draw_reset(); native_draw_tri(1,2,3,4,5,0/0,0,0,0)",false);copy(0);
        run("native_draw_tri(1,2,3,4,5,1e300,0,0,0)",false);copy(0);
        run("native_draw_tri(1,2,3,4,5,6,-1,0,0)",false);copy(0);
        run("native_draw_tri(1,2,3,4,5,6,0x100000000,0,0)",false);copy(0);
        run("native_draw_tri(1,2)",false);copy(0);
        run("for i=1,60000 do native_draw_tri(1,2,3,4,5,6,0,0,0) end");copy(180000);
        run("native_draw_tri(1,2,3,4,5,6,0,0,0)",false);copy(180000);
        bool rejected=false;try{copy(179997);}catch(const std::runtime_error&){rejected=true;}
        if(!rejected)throw std::runtime_error("Mismatched Lua/native count accepted");
        run("native_draw_reset()");copy(0);
        run("native_draw_tri(1,2,3,4,5,6,0,0,0)");copy(3);
        lua_close(L);std::cout<<"PASS native vertex batches: topology, UV, ABGR, AA, malformed/nonfinite payload, capacity, reset and count validation\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';lua_close(L);return 1;}
}
