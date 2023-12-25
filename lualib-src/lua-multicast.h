#ifndef LUA_MULTICAST_H
#define LUA_MULTICAST_H

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}
#include <cstdint>
#include <cstring>

#include <atomic>
#include "fengnet.h"

using namespace std;

extern "C" int luaopen_skynet_multicast_core(lua_State *L);

#endif