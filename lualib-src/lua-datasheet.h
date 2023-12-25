#ifndef LUA_DATASHEET_H
#define LUA_DATASHEET_H

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}
#include <cstdint>

extern "C" int luaopen_fengnet_datasheet_core(lua_State *L);

#endif