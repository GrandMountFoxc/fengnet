#ifndef LUA_SOCKET_H
#define LUA_SOCKET_H

#include <cstdlib>
#include <cstring>
#include <cstdbool>
#include <cstdint>
#include <cassert>

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>

#include "fengnet.h"
#include "fengnet_socket.h"

extern "C" int luaopen_fengnet_socketdriver(lua_State *L);

#endif