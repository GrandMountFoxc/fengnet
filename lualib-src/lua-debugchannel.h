#ifndef LUA_DEBUGCHANNEL_H
#define LUA_DEBUGCHANNEL_H

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "spinlock.h"

using namespace std;

extern "C" int luaopen_fengnet_debugchannel(lua_State *L);

#endif