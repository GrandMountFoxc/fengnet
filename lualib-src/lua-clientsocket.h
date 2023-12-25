#ifndef LUA_CLIENTSOCKET_H
#define LUA_CLIENTSOCKET_H

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}
#include <cstring>
#include <cstdint>
#include <pthread.h>
#include <cstdlib>

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

extern "C" int luaopen_client_socket(lua_State *L);

#endif