#include "lua_vertex_buffer.hpp"
extern "C" {
#include "lualib.h"
}
#include <iostream>
#include "test_support.hpp"

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
        scenario("lua/nonfinite-payload");
        run("native_draw_reset(); native_draw_tri(1,2,3,4,5,0/0,0,0,0)",false);copy(0);
        scenario("lua/capacity-count-reset");
        run("for i=1,60000 do native_draw_tri(1,2,3,4,5,6,0,0,0) end");copy(180000);
        run("native_draw_tri(1,2,3,4,5,6,0,0,0)",false);copy(180000);
        bool rejected=false;try{copy(179997);}catch(const std::runtime_error&){rejected=true;}
        if(!rejected)throw std::runtime_error("Mismatched Lua/native count accepted");
        run("native_draw_reset()");copy(0);
        run("native_draw_tri(1,2,3,4,5,6,0,0,0)");copy(3);
        buffer.detail_quality(1);
        run("native_draw_reset(); for i=1,90000 do native_draw_tri(1,2,3,4,5,6,0,0,0) end");copy(270000);
        run("native_draw_tri(1,2,3,4,5,6,0,0,0)",false);
        buffer.detail_quality(0);
        run("for i=1,100000 do native_draw_tri(1,2,3,4,5,6,0,0,0) end");copy(570000);
        buffer.detail_quality(4);
        run("native_draw_reset(); for i=1,60000 do native_draw_tri(1,2,3,4,5,6,0,0,0) end");copy(180000);
        run("native_draw_tri(1,2,3,4,5,6,0,0,0)",false);
        luaL_openlibs(L);
        if(luaL_dofile(L,"assets/a350/map_engine.lua")!=LUA_OK||
           luaL_dofile(L,"tests/pavement_detail_tests.lua")!=LUA_OK)
            throw std::runtime_error(lua_tostring(L,-1));
        lua_close(L);std::cout<<"PASS native vertex batches: topology, UV, ABGR, AA, malformed/nonfinite payload, capacity, reset and count validation\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';lua_close(L);return 1;}
}
