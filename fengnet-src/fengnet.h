#ifndef FENGNET_H
#define FENGNET_H

#include "fengnet_malloc.h"
#include "fengnet_harbor.h"
#include "fengnet_imp.h"

#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <atomic>
#include <vector>
#include <unordered_map>

#define LOG_MESSAGE_SIZE 256

#define PTYPE_TEXT 0
#define PTYPE_RESPONSE 1
#define PTYPE_MULTICAST 2
#define PTYPE_CLIENT 3
#define PTYPE_SYSTEM 4
#define PTYPE_HARBOR 5
#define PTYPE_SOCKET 6
// read lualib/skynet.lua examples/simplemonitor.lua
#define PTYPE_ERROR 7	
// read lualib/skynet.lua lualib/mqueue.lua lualib/snax.lua
#define PTYPE_RESERVED_QUEUE 8
#define PTYPE_RESERVED_DEBUG 9
#define PTYPE_RESERVED_LUA 10
#define PTYPE_RESERVED_SNAX 11

#define PTYPE_TAG_DONTCOPY 0x10000
#define PTYPE_TAG_ALLOCSESSION 0x20000

#define CMD_MAX_NUM 20

using namespace std;

// class FengnetServer;
// class FengnetTimer;
// class FengnetHandle;
// class FengnetMQ;
// class FengnetModule;
// class FengnetHarbor;

struct fengnet_context;

struct fengnet_node {
	// ATOM_INT是自定义的类型，原型是 volatile int，在 atomic.h 中定义
	// 目前理解是作者自己实现了一套原子操作，暂时还不明白为什么要这么做
	// 后续要考虑是否能用 C++11 当中的 atomic 替换这部分
    //
    // C++11 新特性 atomic<int> 初始化不确定是不是线程安全的
	atomic<int> total;
	int init;
	uint32_t monitor_exit;
	bool profile;	// default is on
};

// skynet command
struct command_func {
	const char* name;
	const char* (*func)(fengnet_context* context, const char* param);
};

typedef int (*fengnet_cb)(fengnet_context * context, void *ud, int type, int session, uint32_t source , const void * msg, size_t sz);

class Fengnet{
public:
    // 单例
    static Fengnet* inst;
    fengnet_node G_NODE;
public:
    Fengnet();
    // 初始化并开始
    void Start();
    const char* fengnet_command(fengnet_context* context, const char* cmd , const char* parm);  

    void fengnet_error(fengnet_context * context, const char *msg, ...);    // fengnet_error.cpp

    int fengnet_send(fengnet_context* context, uint32_t source, uint32_t destination , int type, int session, void * msg, size_t sz);
    int fengnet_sendname(fengnet_context* context, uint32_t source, const char * destination , int type, int session, void * msg, size_t sz);
    int fengnet_isremote(fengnet_context *, uint32_t handle, int * harbor);
    void fengnet_callback(fengnet_context* context, void* ud, fengnet_cb cb);
    uint32_t fengnet_current_handle();
    uint64_t fengnet_now();     // fengnet_timer.cpp
private:
    static const char* cmd_timeout(fengnet_context* context, const char* param);
    static const char* cmd_reg(fengnet_context* context, const char* param);
    static const char* cmd_query(fengnet_context* context, const char* param);
    static const char* cmd_name(fengnet_context* context, const char* param);
    static const char* cmd_exit(fengnet_context* context, const char* param);
    static const char* cmd_kill(fengnet_context* context, const char* param);
    static const char* cmd_launch(fengnet_context* context, const char* param);
    static const char* cmd_getenv(fengnet_context* context, const char* param);
    static const char* cmd_setenv(fengnet_context* context, const char* param);
    static const char* cmd_starttime(fengnet_context* context, const char* param);
    static const char* cmd_abort(fengnet_context* context, const char* param);
    static const char* cmd_monitor(fengnet_context* context, const char* param);
    static const char* cmd_stat(fengnet_context* context, const char* param);
    static const char* cmd_logon(fengnet_context* context, const char* param);
    static const char* cmd_logoff(fengnet_context* context, const char* param);
    static const char* cmd_signal(fengnet_context* context, const char* param);
private:
    // shared_ptr<FengnetServer> fengnetServer;
    // shared_ptr<FengnetTimer> fengnetTimer;
    // shared_ptr<FengnetHandle> fengnetHandle;
    // shared_ptr<FengnetMQ> fengnetMQ;
    // shared_ptr<FengnetHarbor> fengnetHarbor;
    // shared_ptr<FengnetModule> fengnetModule;
    // fengnet可接收的一系列指令
    // 对ctx操作，通常会先调用fengnet_context_grab将引用计数+1，操作完调用skynet_context_release将引用计数-1，以保证操作ctx过程中，不会被其他线程释放掉。
    const static command_func cmd_funcs[CMD_MAX_NUM];
private:
    void handle_exit(fengnet_context* context, uint32_t handle);
    uint32_t tohandle(fengnet_context* context, const char* param);
    void id_to_hex(char* str, uint32_t id);
    void _filter_args(fengnet_context * context, int type, int *session, void ** data, size_t * sz);
    void copy_name(char name[GLOBALNAME_LENGTH], const char * addr);
};

#endif