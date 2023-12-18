#include "fengnet.h"

extern "C"{
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <atomic>

#if defined(__APPLE__)
#include <mach/task.h>
#include <mach/mach.h>
#endif

#define NANOSEC 1000000000
#define MICROSEC 1000000

// #define DEBUG_LOG
// 内存阈值，当 snlua 占用的内存超过阈值则触发警报
#define MEMORY_WARNING_REPORT (1024 * 1024 * 32)

struct snlua {
	lua_State * L;					// 每个 snlua 模块都配备了专属的 lua 环境
									// 不同的 snlua 服务将不同的 lua 脚本运行在自己的 lua 环境中，彼此之间互不影响
	fengnet_context* ctx;	// 模块所属的服务
	size_t mem;
	size_t mem_report;				// 内存阈值
	size_t mem_limit;
	lua_State * activeL;
	atomic<int> trap;
};

// LUA_CACHELIB may defined in patched lua for shared proto
#ifdef LUA_CACHELIB

#define codecache luaopen_cache

#else

static int cleardummy(lua_State *L) {
  return 0;
}

static int codecache(lua_State *L) {
	luaL_Reg l[] = {
		{ "clear", cleardummy },
		{ "mode", cleardummy },
		{ NULL, NULL },
	};
	luaL_newlib(L,l);
	lua_getglobal(L, "loadfile");
	lua_setfield(L, -2, "loadfile");
	return 1;
}

#endif

static void signal_hook(lua_State *L, lua_Debug *ar) {
	void *ud = NULL;
	lua_getallocf(L, &ud);
	snlua *l = (snlua *)ud;

	lua_sethook (L, NULL, 0, 0);
	if (atomic_load(&l->trap)) {
		atomic_store(&l->trap , 0);
		luaL_error(L, "signal 0");
	}
}

static void
switchL(lua_State *L, snlua *l) {
	l->activeL = L;
	if (atomic_load(&l->trap)) {
		lua_sethook(L, signal_hook, LUA_MASKCOUNT, 1);
	}
}

static int
lua_resumeX(lua_State *L, lua_State *from, int nargs, int *nresults) {
	void *ud = NULL;
	lua_getallocf(L, &ud);
	snlua *l = (snlua *)ud;
	switchL(L, l);
	int err = lua_resume(L, from, nargs, nresults);
	if (atomic_load(&l->trap)) {
		// wait for lua_sethook. (l->trap == -1)
		while (atomic_load(&l->trap) >= 0) ;
	}
	switchL(from, l);
	return err;
}

static double get_time() {
#if  !defined(__APPLE__)
	timespec ti;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ti);

	int sec = ti.tv_sec & 0xffff;
	int nsec = ti.tv_nsec;

	return (double)sec + (double)nsec / NANOSEC;
#else
	struct task_thread_times_info aTaskInfo;
	mach_msg_type_number_t aTaskInfoCount = TASK_THREAD_TIMES_INFO_COUNT;
	if (KERN_SUCCESS != task_info(mach_task_self(), TASK_THREAD_TIMES_INFO, (task_info_t )&aTaskInfo, &aTaskInfoCount)) {
		return 0;
	}

	int sec = aTaskInfo.user_time.seconds & 0xffff;
	int msec = aTaskInfo.user_time.microseconds;

	return (double)sec + (double)msec / MICROSEC;
#endif
}

static inline double diff_time(double start) {
	double now = get_time();
	if (now < start) {
		return now + 0x10000 - start;
	} else {
		return now - start;
	}
}

// coroutine lib, add profile

/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (lua_State *L, lua_State *co, int narg) {
  int status, nres;
  if (!lua_checkstack(co, narg)) {
    lua_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  lua_xmove(L, co, narg);
  status = lua_resumeX(co, L, narg, &nres);
  if (status == LUA_OK || status == LUA_YIELD) {
    if (!lua_checkstack(L, nres + 1)) {
      lua_pop(co, nres);  /* remove results anyway */
      lua_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    lua_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    lua_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}

static int timing_enable(lua_State *L, int co_index, lua_Number *start_time) {
	lua_pushvalue(L, co_index);
	lua_rawget(L, lua_upvalueindex(1));
	if (lua_isnil(L, -1)) {		// check total time
		lua_pop(L, 1);
		return 0;
	}
	*start_time = lua_tonumber(L, -1);
	lua_pop(L,1);
	return 1;
}

static double timing_total(lua_State *L, int co_index) {
	lua_pushvalue(L, co_index);
	lua_rawget(L, lua_upvalueindex(2));
	double total_time = lua_tonumber(L, -1);
	lua_pop(L,1);
	return total_time;
}

static int timing_resume(lua_State *L, int co_index, int n) {
	lua_State *co = lua_tothread(L, co_index);
	lua_Number start_time = 0;
	if (timing_enable(L, co_index, &start_time)) {
		start_time = get_time();
#ifdef DEBUG_LOG
		fprintf(stderr, "PROFILE [%p] resume %lf\n", co, ti);
#endif
		lua_pushvalue(L, co_index);
		lua_pushnumber(L, start_time);
		lua_rawset(L, lua_upvalueindex(1));	// set start time
	}

	int r = auxresume(L, co, n);

	if (timing_enable(L, co_index, &start_time)) {
		double total_time = timing_total(L, co_index);
		double diff = diff_time(start_time);
		total_time += diff;
#ifdef DEBUG_LOG
		fprintf(stderr, "PROFILE [%p] yield (%lf/%lf)\n", co, diff, total_time);
#endif
		lua_pushvalue(L, co_index);
		lua_pushnumber(L, total_time);
		lua_rawset(L, lua_upvalueindex(2));
	}

	return r;
}

static int luaB_coresume (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTHREAD);
  int r = timing_resume(L, 1, lua_gettop(L) - 1);
  if (r < 0) {
    lua_pushboolean(L, 0);
    lua_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    lua_pushboolean(L, 1);
    lua_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}

static int luaB_auxwrap (lua_State *L) {
  lua_State *co = lua_tothread(L, lua_upvalueindex(3));
  int r = timing_resume(L, lua_upvalueindex(3), lua_gettop(L));
  if (r < 0) {
    int stat = lua_status(co);
    if (stat != LUA_OK && stat != LUA_YIELD)
      lua_closethread(co, L);  /* close variables in case of errors */
    if (lua_type(L, -1) == LUA_TSTRING) {  /* error object is a string? */
      luaL_where(L, 1);  /* add extra info, if available */
      lua_insert(L, -2);
      lua_concat(L, 2);
    }
    return lua_error(L);  /* propagate error */
  }
  return r;
}

static int luaB_cocreate (lua_State *L) {
  lua_State *NL;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  NL = lua_newthread(L);
  lua_pushvalue(L, 1);  /* move function to top */
  lua_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}

static int luaB_cowrap (lua_State *L) {
  lua_pushvalue(L, lua_upvalueindex(1));
  lua_pushvalue(L, lua_upvalueindex(2));
  luaB_cocreate(L);
  lua_pushcclosure(L, luaB_auxwrap, 3);
  return 1;
}

// profile lib

static int lstart(lua_State *L) {
	if (lua_gettop(L) != 0) {
		lua_settop(L,1);
		luaL_checktype(L, 1, LUA_TTHREAD);
	} else {
		lua_pushthread(L);
	}
	lua_Number start_time = 0;
	if (timing_enable(L, 1, &start_time)) {
		return luaL_error(L, "Thread %p start profile more than once", lua_topointer(L, 1));
	}

	// reset total time
	lua_pushvalue(L, 1);
	lua_pushnumber(L, 0);
	lua_rawset(L, lua_upvalueindex(2));

	// set start time
	lua_pushvalue(L, 1);
	start_time = get_time();
#ifdef DEBUG_LOG
	fprintf(stderr, "PROFILE [%p] start\n", L);
#endif
	lua_pushnumber(L, start_time);
	lua_rawset(L, lua_upvalueindex(1));

	return 0;
}

static int lstop(lua_State *L) {
	if (lua_gettop(L) != 0) {
		lua_settop(L,1);
		luaL_checktype(L, 1, LUA_TTHREAD);
	} else {
		lua_pushthread(L);
	}
	lua_Number start_time = 0;
	if (!timing_enable(L, 1, &start_time)) {
		return luaL_error(L, "Call profile.start() before profile.stop()");
	}
	double ti = diff_time(start_time);
	double total_time = timing_total(L,1);

	lua_pushvalue(L, 1);	// push coroutine
	lua_pushnil(L);
	lua_rawset(L, lua_upvalueindex(1));

	lua_pushvalue(L, 1);	// push coroutine
	lua_pushnil(L);
	lua_rawset(L, lua_upvalueindex(2));

	total_time += ti;
	lua_pushnumber(L, total_time);
#ifdef DEBUG_LOG
	fprintf(stderr, "PROFILE [%p] stop (%lf/%lf)\n", lua_tothread(L,1), ti, total_time);
#endif

	return 1;
}

static int init_profile(lua_State *L) {
	luaL_Reg l[] = {
		{ "start", lstart },
		{ "stop", lstop },
		{ "resume", luaB_coresume },
		{ "wrap", luaB_cowrap },
		{ NULL, NULL },
	};
	luaL_newlibtable(L,l);
	lua_newtable(L);	// table thread->start time
	lua_newtable(L);	// table thread->total time

	lua_newtable(L);	// weak table
	lua_pushliteral(L, "kv");
	lua_setfield(L, -2, "__mode");

	lua_pushvalue(L, -1);
	lua_setmetatable(L, -3);
	lua_setmetatable(L, -3);

	luaL_setfuncs(L,l,2);

	return 1;
}

/// end of coroutine

static int traceback (lua_State *L) {
	const char *msg = lua_tostring(L, 1);
	if (msg)
		// void luaL_traceback (lua_State *L, lua_State *L1, const char *msg, int level)
		// Creates and pushes a traceback of the stack L1. 
		// If msg is not NULL it is appended at the beginning of the traceback. 
		// The level parameter tells at which level to start the traceback.
		// 看起来是用来打印或者记录报错的
		luaL_traceback(L, L, msg, 1);
	else {
		lua_pushliteral(L, "(no error message)");
	}
	return 1;
}

static void report_launcher_error(struct fengnet_context *ctx) {
	// sizeof "ERROR" == 5
	Fengnet::inst->fengnet_sendname(ctx, 0, ".launcher", PTYPE_TEXT, 0, (char*)"ERROR", 5);
}

// optstring更多地感觉是去避免 GETENV 函数没获取到对应的环境配置返回默认值 str 
static const char* optstring(fengnet_context *ctx, const char *key, const char * str) {
	const char * ret = Fengnet::inst->fengnet_command(ctx, "GETENV", key);
	if (ret == NULL) {
		return str;
	}
	return ret;
}

// init_cb 中最主要的部分便是设置相应的环境变量以及加载器loader
static int init_cb(snlua *l, fengnet_context *ctx, const char * args, size_t sz) {
	lua_State *L = l->L;
	l->ctx = ctx;
	// 暂停 lua 的 GC 机制
	lua_gc(L, LUA_GCSTOP, 0);
	lua_pushboolean(L, 1);  /* signal for libraries to ignore env. vars. */
	// void lua_setfield (lua_State *L, int index, const char *k)
	// Does the equivalent to t[k] = v, where t is the value at the given index and v is the value at the top of the stack.
	// 这个函数将把这个值弹出堆栈。 跟在 Lua 中一样，这个函数可能触发一个 "newindex" 事件的元方法
	// 也就是把 L 中 index=LUA_REGISTRYINDEX 位置的变量（可能是一个哈希表） "LUA_NOENV" 对应的值赋值为栈顶 value，也就是刚 lua_pushboolean(L, 1) push 的1
	lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");
	luaL_openlibs(L);
	// void luaL_requiref (lua_State *L, const char *modname, lua_CFunction openf, int glb);
	// 这个函数还是不太理解作用是什么
	// https://blog.csdn.net/Mr_sandman1994/article/details/110233572
	// 但从这个博客上的例子来看应该是注册这个skynet.profile库
	luaL_requiref(L, "fengnet.profile", init_profile, 0);

	int profile_lib = lua_gettop(L);
	// replace coroutine.resume / coroutine.wrap
	lua_getglobal(L, "coroutine");	// 查找coroutine变量压入栈顶
	// void lua_getfield (lua_State *L, int index, const char *k)
	// Pushes onto the stack the value t[k], where t is the value at the given index.
	// 把 t[k] 值压入堆栈， 这里的 t 是指有效索引 index 指向的值。 在 Lua 中，这个函数可能触发对应 "index" 事件的元方法
	// 这里应该就是把 skynet.profile 里面注册的 resume 函数加入栈顶
	lua_getfield(L, profile_lib, "resume");
	lua_setfield(L, -2, "resume");
	lua_getfield(L, profile_lib, "wrap");
	lua_setfield(L, -2, "wrap");

	lua_settop(L, profile_lib-1);

	lua_pushlightuserdata(L, ctx);
	lua_setfield(L, LUA_REGISTRYINDEX, "fengnet_context");
	// 判断 skynet.codecache 是否为与 package.loaded 当中。如果不在则调用 codecache 进行加载
	luaL_requiref(L, "fengnet.codecache", codecache , 0);
	lua_pop(L,1);

	lua_gc(L, LUA_GCGEN, 0, 0);
	// 设置相关的全局变量
	const char *path = optstring(ctx, "lua_path","./lualib/?.lua;./lualib/?/init.lua");
	lua_pushstring(L, path);
	// void lua_setglobal (lua_State *L, const char *name)
	// Pops a value from the stack and sets it as the new value of global name
	// 栈顶元素设置为名为 name 的全局变量
	lua_setglobal(L, "LUA_PATH");	// Lua搜索路径，在config.lua_path指定
	const char *cpath = optstring(ctx, "lua_cpath","./luaclib/?.so");
	lua_pushstring(L, cpath);
	lua_setglobal(L, "LUA_CPATH");	// C模块的搜索路径，在config.lua_cpath指定
	const char *service = optstring(ctx, "luaservice", "./service/?.lua");
	lua_pushstring(L, service);
	lua_setglobal(L, "LUA_SERVICE");// Lua服务的搜索路径，在config.luaservice指定
	const char *preload = Fengnet::inst->fengnet_command(ctx, "GETENV", "preload");
	lua_pushstring(L, preload);
	lua_setglobal(L, "LUA_PRELOAD");// 预加载脚本，这些脚本会在所有服务开始之前执行，可以用它来初始化一些全局的设置

	// traceback 将 L 栈的回溯信息压入栈
	lua_pushcfunction(L, traceback);
	assert(lua_gettop(L) == 1);

	// lua 服务的加载器为 loader.lua
	const char * loader = optstring(ctx, "lualoader", "./lualib/loader.lua");

	int r = luaL_loadfile(L,loader);
	if (r != LUA_OK) {
		Fengnet::inst->fengnet_error(ctx, "Can't load %s : %s", loader, lua_tostring(L, -1));
		report_launcher_error(ctx);
		return 1;
	}
	// loader.lua 的主要作用是对环境变量以及传入的参数进行一些文本处理，然后找到对应的文件去执行，
	// 这里的参数主要是指 bootstrap，最终会执行 /service/bootstrap.lua 文件
	// args = bootstrap
	lua_pushlstring(L, args, sz);
	// 利用 loader 将 bootstrap.lua 脚本执行起来
	r = lua_pcall(L,1,0,1);
	if (r != LUA_OK) {
		Fengnet::inst->fengnet_error(ctx, "lua loader error : %s", lua_tostring(L, -1));
		report_launcher_error(ctx);
		return 1;
	}
	lua_settop(L,0);
	if (lua_getfield(L, LUA_REGISTRYINDEX, "memlimit") == LUA_TNUMBER) {
		size_t limit = lua_tointeger(L, -1);
		l->mem_limit = limit;
		Fengnet::inst->fengnet_error(ctx, "Set memory limit to %.2f M", (float)limit / (1024 * 1024));
		lua_pushnil(L);
		lua_setfield(L, LUA_REGISTRYINDEX, "memlimit");
	}
	lua_pop(L, 1);

	// 重启 lua 的 GC 机制
	lua_gc(L, LUA_GCRESTART, 0);

	return 0;
}

// msg 的值为 bootstrap
static int launch_cb(fengnet_context * context, void *ud, int type, int session, uint32_t source , const void* msg, size_t sz) {
	assert(type == 0 && session == 0);
	snlua *l = (snlua*)ud;
	// 重设回调函数
	Fengnet::inst->fengnet_callback(context, NULL, NULL);
	int err = init_cb(l, context, (const char*)msg, sz);
	if (err) {
		Fengnet::inst->fengnet_command(context, "EXIT", NULL);
	}

	return 0;
}

extern "C" int snlua_init(snlua *l, fengnet_context *ctx, const char * args) {
	int sz = strlen(args);
	char * tmp = new char[sz];
	memcpy(tmp, args, sz);
	// 将 launch_cb 设置为 snlua 服务的回调函数，参数为 l
	Fengnet::inst->fengnet_callback(ctx, l , launch_cb);
	const char * self = Fengnet::inst->fengnet_command(ctx, "REG", NULL);
	// self 的值为 :handle
	uint32_t handle_id = strtoul(self+1, NULL, 16);
	// it must be first message
	// 向自己发送第一条消息，这条消息将由 launch_cb 进行处理，消息内容为 "bootstrap"
	Fengnet::inst->fengnet_send(ctx, 0, handle_id, PTYPE_TAG_DONTCOPY,0, tmp, sz);
	return 0;
}

static void* lalloc(void * ud, void *ptr, size_t osize, size_t nsize) {
	snlua *l = (snlua*)ud;
	size_t mem = l->mem;
	l->mem += nsize;
	if (ptr)
		l->mem -= osize;
	if (l->mem_limit != 0 && l->mem > l->mem_limit) {
		if (ptr == NULL || nsize > osize) {
			l->mem = mem;
			return NULL;
		}
	}
	if (l->mem > l->mem_report) {
		l->mem_report *= 2;
		Fengnet::inst->fengnet_error(l->ctx, "Memory warning %.2f M", (float)l->mem / (1024 * 1024));
	}
	return fengnet_lalloc(ptr, osize, nsize);
}

extern "C" snlua* snlua_create() {
	snlua * l = new snlua();
	memset(l,0,sizeof(*l));
	l->mem_report = MEMORY_WARNING_REPORT;
	l->mem_limit = 0;
	// lua_State *lua_newstate (lua_Alloc f, void *ud);
	// The argument f is the allocator function
	// The second argument, ud, is an opaque pointer that Lua passes to the allocator in every call.
	// Creates a new thread running in a new, independent state.
	// Returns NULL if it cannot create the thread or the state (due to lack of memory).
	l->L = lua_newstate(lalloc, l);		// 从这行代码看lua_State的含义就清晰了，说白了就是开一个新线程然后分配一块内存用来执行lua脚本
	l->activeL = NULL;
	l->trap = 0;
	return l;
}

extern "C" void snlua_release(snlua *l) {
	lua_close(l->L);
	delete l;
}

extern "C" void snlua_signal(snlua *l, int signal) {
	Fengnet::inst->fengnet_error(l->ctx, "recv a signal %d", signal);
	if (signal == 0) {
		if (atomic_load(&l->trap) == 0) {
			// only one thread can set trap ( l->trap 0->1 )
			// if (!ATOM_CAS(&l->trap, 0, 1))
            int excepted_val = 0;
            if(!l->trap.compare_exchange_weak(excepted_val, 1))
				return;
			lua_sethook (l->activeL, signal_hook, LUA_MASKCOUNT, 1);
			// finish set ( l->trap 1 -> -1 )
			// ATOM_CAS(&l->trap, 1, -1);
            excepted_val = 1;
            l->trap.compare_exchange_weak(excepted_val, -1);
		}
	} else if (signal == 1) {
		Fengnet::inst->fengnet_error(l->ctx, "Current Memory %.3fK", (float)l->mem / 1024);
	}
}
