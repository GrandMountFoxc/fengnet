#ifndef LUA_SHARETABLE_H
#define LUA_SHARETABLE_H

extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

#include "lgc.h"

extern "C" int luaopen_fengnet_sharetable_core(lua_State *L);

#endif