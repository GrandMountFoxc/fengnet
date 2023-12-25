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
    #include "lua.h"
}

#include "fengnet.h"
#include "fengnet_handle.h"

extern "C" size_t malloc_used_memory(void);
extern "C" size_t malloc_memory_block(void);
extern "C" void   memory_info_dump(const char *opts);
extern "C" size_t mallctl_int64(const char* name, size_t* newval);
extern "C" int    mallctl_opt(const char* name, int* newval);
extern "C" bool   mallctl_bool(const char* name, bool* newval);
extern "C" int    mallctl_cmd(const char* name);
extern "C" void   dump_c_mem(void);
extern "C" int    dump_mem_lua(lua_State *L);
extern "C" size_t malloc_current_memory(void);

#endif /* FENGNET_MALLOC_HOOK_H */
