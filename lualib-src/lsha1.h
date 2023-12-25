#ifndef LSHA_H
#define LSHA_H

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}

#include <cstdio>
#include <cstring>
#include <cstdint>

extern "C" int lsha1(lua_State *L);
extern "C" int lhmac_sha1(lua_State *L);

#endif