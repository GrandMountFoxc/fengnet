#ifndef LUA_CRYPT_H
#define LUA_CRYPT_H

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}

#include <ctime>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

extern "C" int luaopen_fengnet_crypt(lua_State *L);
extern "C" int luaopen_client_crypt(lua_State *L);

#endif