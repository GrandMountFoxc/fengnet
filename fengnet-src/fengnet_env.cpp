#include "fengnet_env.h"

FengnetEnv* FengnetEnv::inst;
FengnetEnv::FengnetEnv(){
    inst = this;
}

const char* FengnetEnv::fengnet_getenv(const char* key){
    lock_guard<SpinLock> guard(lock);

    // lua_State* L = L;

    // Pushes onto the stack the value of the global name. Returns the type of that value.
	// lua_getglobal的作用就是把输入的变量名对应的数据放入栈顶。
    lua_getglobal(L, key);
    const char* result = lua_tostring(L, -1);
    lua_pop(L, 1);

    return result;
}

void FengnetEnv::fengnet_setenv(const char* key, const char* value){
    lock_guard<SpinLock> guard(lock);

    // lua_State* L = L;
    // 再查看一次key是否存在，如果不存在也就是nil再插入对应值
    lua_getglobal(L, key);
    assert(lua_isnil(L, -1));	// assert(true)继续执行
    lua_pop(L, 1);
	// 插入对应值
    lua_pushstring(L, value);
	// Pops a value from the stack and sets it as the new value of global name.
	// lua_setglobal的功能是，pop出栈顶的数据，并使用给定的名字把它设置为全局变量
    lua_setglobal(L, key);
}

void FengnetEnv::fengnet_env_init(){
    // 这里应该对 new 调用的分配器进行指定，需要用 fengnet_malloc进行内存分配
	// skynet_malloc 对 malloc 进行了重写
	// 具体的细节在 malloc_hook 当中实现的
    L = luaL_newstate();
}