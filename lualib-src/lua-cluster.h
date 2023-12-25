#ifndef LUA_CLUSTER_H
#define LUA_CLUSTER_H

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}
#include <cstring>
#include <cassert>
#include <unistd.h>

#include "fengnet.h"

extern "C" int luaopen_fengnet_cluster_core(lua_State *L);

#endif