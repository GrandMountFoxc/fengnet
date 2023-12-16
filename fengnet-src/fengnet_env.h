#ifndef FENGNET_ENV_H
#define FENGNET_ENV_H

extern "C"{
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

#include <cstdlib>
#include <cassert>
#include <mutex>

#include "spinlock.h"
#include "fengnet.h"

class FengnetEnv{
public:
    // 单例
    static FengnetEnv* inst;
    lua_State* L;
public:
    // 构造函数
    FengnetEnv();
    FengnetEnv(const FengnetEnv&) = delete;
    FengnetEnv& operator=(const FengnetEnv&) = delete;
    const char* fengnet_getenv(const char* key);
    void fengnet_setenv(const char* key, const char* value);

    void fengnet_env_init();
private:
    SpinLock lock;
};

#endif