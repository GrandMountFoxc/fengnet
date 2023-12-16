extern "C"{
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

#include "fengnet.h"

#include "fengnet_imp.h"
#include "fengnet_env.h"
#include "fengnet_server.h"

#include <iostream>
// #include <stdlib.h>
#include <cstdlib>
// #include <string.h>
#include <cstring>
// #include <signal.h>
#include <csignal>
// #include <assert.h>
#include <cassert>
#include <memory>

using namespace std;

// 为什么要返回一个数字？
// 三个opt函数一起看，感觉就是一个转换函数

// 在lua栈当中找key对应的值，如果没有则返回opt（默认值）
static int optint(const char* key, int opt){
    const char* str = FengnetEnv::inst->fengnet_getenv(key);  // 从lua栈中取元素
    if(str==nullptr){
        char tmp[20];
        sprintf(tmp, "%d", opt);
        FengnetEnv::inst->fengnet_setenv(key, tmp);
        return opt;
    }
    return strtol(str, nullptr, 10);
}

static const char* optstring(const char* key, const char* opt){
    const char* str = FengnetEnv::inst->fengnet_getenv(key);  // 从lua栈中取元素
    if(str==nullptr){
        if(opt){
            FengnetEnv::inst->fengnet_setenv(key, opt);
            opt = FengnetEnv::inst->fengnet_getenv(key);
        }
        return opt;
    }
    return str;
}

static int optboolean(const char* key, int opt){
    const char* str = FengnetEnv::inst->fengnet_getenv(key);  // 从lua栈中取元素
    if(str==nullptr){
        FengnetEnv::inst->fengnet_setenv(key, opt?"true":"false");
        return opt;
    }
    return strcmp(str, "true")==0;
}

// 遍历Lua栈中的table来初始化参数
// 参数包括（cpath:"./cservice/?.so", 
// bootstrap:"snlua bootstrap", 
// lua_cpath:"./luaclib/?.so", 
// lualoader:"./lualib/loader.lua", 
// thread:8, harbor:0, 
// lua_cpath:"./lualib/?.lua;./lualib/?/init.lua", 
// luaservice:"./service/?.lua;./test/?.lua;./examples/?.lua;./test/?/init.lua", 
// start:main)
static void _init_env(lua_State* L){
    // 遍历lua栈
    lua_pushnil(L);
    while(lua_next(L, -2)!=0){
        int keyt = lua_type(L, -2);
        if(keyt != LUA_TSTRING){
            fprintf(stderr, "Invalid config table\n");
            exit(1);
        }
        const char* key = lua_tostring(L, -2);
        if(lua_type(L, -1)==LUA_TBOOLEAN){
            int b = lua_toboolean(L, -1);
            FengnetEnv::inst->fengnet_setenv(key, b?"true":"false");
        }else{
            const char* value = lua_tostring(L, -1);
            if(value==nullptr){
                fprintf(stderr, "Invalid config table key = %s\n", key);
                exit(1);
            }
            FengnetEnv::inst->fengnet_setenv(key, value);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

// 忽略PIPE信号
int sigign(){
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPIPE, &sa, 0);
    return 0;
}

static const char * load_config = "\
	local result = {}\n\
	local function getenv(name) return assert(os.getenv(name), [[os.getenv() failed: ]] .. name) end\n\
	local sep = package.config:sub(1,1)\n\
	local current_path = [[.]]..sep\n\
	local function include(filename)\n\
		local last_path = current_path\n\
		local path, name = filename:match([[(.*]]..sep..[[)(.*)$]])\n\
		if path then\n\
			if path:sub(1,1) == sep then	-- root\n\
				current_path = path\n\
			else\n\
				current_path = current_path .. path\n\
			end\n\
		else\n\
			name = filename\n\
		end\n\
		local f = assert(io.open(current_path .. name))\n\
		local code = assert(f:read [[*a]])\n\
		code = string.gsub(code, [[%$([%w_%d]+)]], getenv)\n\
		f:close()\n\
		assert(load(code,[[@]]..filename,[[t]],result))()\n\
		current_path = last_path\n\
	end\n\
	setmetatable(result, { __index = { include = include } })\n\
	local config_name = ...\n\
	include(config_name)\n\
	setmetatable(result, nil)\n\
	return result\n\
";

int main(int argc, char* argv[]){
    const char* config_file = nullptr;
    if(argc>1){
        config_file = argv[1];
    }else{
        fprintf(stderr, "Need a config file.\n");
        return 1;
    }

    new Fengnet();
    Fengnet::inst->Start();
    new FengnetEnv();
    FengnetEnv::inst->fengnet_env_init();
    new FengnetServer();
    FengnetServer::serverInst->fengnet_globalinit();

    sigign();

    fengnet_config config;
    
#ifdef LUA_CACHELIB
	// init the lock of code cache
	luaL_initcodecache();
#endif

    lua_State *L = luaL_newstate();
	luaL_openlibs(L);	// link lua lib

	// lua_load 把一段缓存加载为一个 Lua 代码块
	// Loads a Lua chunk without running it. If there are no errors, 
	// lua_load pushes the compiled chunk as a Lua function on top of the stack. 
	// Otherwise, it pushes an error message.
	// int luaL_loadbufferx (lua_State *L, const char *buff, size_t sz, const char *name, const char *mode);
	// mode分"t","b"和"bt"三种分别表示读text，读binary和两者皆可以
	// name is the chunk name, used for debug information and error messages.
	int err =  luaL_loadbufferx(L, load_config, strlen(load_config), "=[fengnet config]", "t");
	assert(err == LUA_OK);
	lua_pushstring(L, config_file);

	// int lua_pcall (lua_State *L, int nargs, int nresults, int msgh)
	// 调用lua栈中的函数
	// nargs表示使用参数数
	// nresults表示返回值数
	// If there are no errors during the call, lua_pcall behaves exactly like lua_call
	// 如果出错会返回错误号，而且在lua栈中压入错误对象
	err = lua_pcall(L, 1, 1, 0);	// 应该是把luaL_loadbufferx加载得到的配置作为函数，config_file作为输入参数执行
	if (err) {
		fprintf(stderr,"%s\n",lua_tostring(L,-1));
		lua_close(L);
		return 1;
	}
    _init_env(L);

    // 这部分还是在初始化skynet_config
	config.thread =  optint("thread",8);
	config.module_path = optstring("cpath","./cservice/?.so");
	config.harbor = optint("harbor", 1);
	config.bootstrap = optstring("bootstrap","snlua bootstrap");
	config.daemon = optstring("daemon", nullptr);		// daemon 守护进程
	config.logger = optstring("logger", nullptr);
	config.logservice = optstring("logservice", "logger");
	config.profile = optboolean("profile", 1);

	lua_close(L);

    fengnet_start(&config);	// skynet_start.c
	FengnetServer::serverInst->fengnet_globalexit();

	return 0;
}