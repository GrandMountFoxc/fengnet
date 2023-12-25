#ifndef LUA_FENGNET_H
#define LUA_FENGNET_H

#include "fengnet.h"
#include "lua-seri.h"

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <inttypes.h>

#include <ctime>

#if defined(__APPLE__)
#include <sys/time.h>
#endif

extern "C" int luaopen_libfengnet_core(lua_State *L);

#endif