#ifndef LUA_STM_H
#define LUA_STM_H

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}
#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <cstring>

#include <shared_mutex>
#include <atomic>

using namespace std;

extern "C" int luaopen_fengnet_stm(lua_State *L);

#endif