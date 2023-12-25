#ifndef LUA_BSON_H
#define LUA_BSON_H

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}

#include <ctime>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdbool>
#include <atomic>

using namespace std;

extern "C" int luaopen_libbson(lua_State *L);

#endif