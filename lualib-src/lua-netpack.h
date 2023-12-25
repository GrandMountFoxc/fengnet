#ifndef LUA_NETPACK_H
#define LUA_NETPACK_H

#include "fengnet_socket.h"

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" int luaopen_fengnet_netpack(lua_State *L);

#endif