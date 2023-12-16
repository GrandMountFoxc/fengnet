#ifndef FENGNET_MALLOC_HOOK_H
#define FENGNET_MALLOC_HOOK_H

#include <cstdlib>
#include <cstdbool>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <atomic>

extern "C"{
    #include <lua.h>
}

#include "fengnet.h"

extern size_t malloc_used_memory(void);
extern size_t malloc_memory_block(void);
extern void   memory_info_dump(const char *opts);
extern size_t mallctl_int64(const char* name, size_t* newval);
extern int    mallctl_opt(const char* name, int* newval);
extern bool   mallctl_bool(const char* name, bool* newval);
extern int    mallctl_cmd(const char* name);
extern void   dump_c_mem(void);
extern int    dump_mem_lua(lua_State *L);
extern size_t malloc_current_memory(void);

#endif /* FENGNET_MALLOC_HOOK_H */
