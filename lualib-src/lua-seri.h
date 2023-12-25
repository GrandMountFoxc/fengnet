#ifndef LUA_SERIALIZE_H
#define LUA_SERIALIZE_H

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}
#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <cstring>

extern "C" int luaseri_pack(lua_State *L);
extern "C" int luaseri_unpack(lua_State *L);

#endif
