#ifndef LUA_SHAREDATA_H
#define LUA_SHAREDATA_H

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <atomic>

using namespace std;

extern "C" int luaopen_fengnet_sharedata_core(lua_State *L);

#endif