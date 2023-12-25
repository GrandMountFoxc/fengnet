#ifndef LUA_MONGO_H
#define LUA_MONGO_H

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>

extern "C" int luaopen_fengnet_mongo_driver(lua_State *L);

#endif