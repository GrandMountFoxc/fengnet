#ifndef LUA_MEMEORY_H
#define LUA_MEMEORY_H

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}

#include "malloc_hook.h"

extern "C" int luaopen_fengnet_memory(lua_State *L);

#endif