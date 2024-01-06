#ifndef FENGNET_SERVER_H
#define FENGNET_SERVER_H

#define CALLING_CHECK

#ifdef CALLING_CHECK

#define CHECKCALLING_BEGIN(ctx) if (!(spinlock_trylock(&ctx->calling))) { assert(0); }
#define CHECKCALLING_END(ctx) spinlock_unlock(&ctx->calling);
#define CHECKCALLING_INIT(ctx) spinlock_init(&ctx->calling);
#define CHECKCALLING_DESTROY(ctx) spinlock_destroy(&ctx->calling);
#define CHECKCALLING_DECL struct spinlock calling;

// #define CHECKCALLING_BEGIN(ctx) if (!(ctx->calling.trylock())) { assert(0); }
// #define CHECKCALLING_END(ctx) ctx->calling.unlock();
// #define CHECKCALLING_INIT(ctx) spinlock_init(&ctx->calling);
// #define CHECKCALLING_DESTROY(ctx) spinlock_destroy(&ctx->calling);
// #define CHECKCALLING_DECL SpinLock calling;

// 直接加锁会死锁？
// #define CHECKCALLING_BEGIN(lock) lock.lock();   // if (!(lock.trylock())) { assert(0); }
// #define CHECKCALLING_END(lock) lock.unlock();

#else

#define CHECKCALLING_BEGIN(ctx)
#define CHECKCALLING_END(ctx)
#define CHECKCALLING_INIT(ctx)
#define CHECKCALLING_DESTROY(ctx)
#define CHECKCALLING_DECL

#endif

// #include "fengnet_malloc.h"
#include "fengnet_harbor.h"
#include "fengnet.h"
#include "fengnet_env.h"

#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <memory>

struct fengnet_context {		// 一个skynet服务ctx的结构
	void * instance;			// 每个ctx自己的数据块，不同类型ctx有不同数据结构，相同类型ctx数据结构相同，但具体的数据不一样，由指定module的create函数返回
	struct fengnet_module * mod;	// 保存module的指针，方便之后调用create,init,signal,release
	void * cb_ud;				// 回调函数参数，可以是NULL
	fengnet_cb cb;				// 回调函数指针，通常在module的init里设置
	struct message_queue *queue;// ctx自己的消息队列指针
	atomic<uintptr_t> logfile;		// 日志句柄
	uint64_t cpu_cost;			// in microsec
	uint64_t cpu_start;			// in microsec
	char result[32];			// 保存skynet_command操作后的结果
	uint32_t handle;			// 标识唯一的ctx id
	int session_id;				// 本方发出请求会设置一个对应的session，当收到对方消息返回时，通过session匹配是哪一个请求的返回
	atomic<int> ref;				// 引用计数，当为0，可以删除ctx
	int message_count;			// 累计收到的消息数量
	bool init;					// 标志位 是否完成初始化
	bool endless;				// 标志位 是否存在死循环
	bool profile;				// 标志位 是否需要开启性能监测

	CHECKCALLING_DECL			// 自旋锁
};

// class FengnetModule;
// class FengnetMQ;
// class FengnetTimer;
// class FengnetHandle;
// class FengnetLog;

struct fengnet_message;
struct fengnet_monitor;
struct message_queue;

class FengnetServer{
public:
    static FengnetServer* serverInst;
public:
    FengnetServer();
    FengnetServer(FengnetServer&) = delete;
    FengnetServer& operator=(FengnetServer&) = delete;
    void fengnet_globalinit();
    void fengnet_globalexit();
    void fengnet_initthread(int m);

    // 这块都跟context有关，之后可能要重新封装
    fengnet_context * fengnet_context_new(const char * name, const char * parm);
    void fengnet_context_grab(fengnet_context *);
    void fengnet_context_reserve(fengnet_context *ctx);
    fengnet_context * fengnet_context_release(fengnet_context *);
    uint32_t fengnet_context_handle(fengnet_context *);
    int fengnet_context_push(uint32_t handle, fengnet_message *message);
    void fengnet_context_send(fengnet_context * context, void * msg, size_t sz, uint32_t source, int type, int session);
    int fengnet_context_newsession(fengnet_context *);
    message_queue* fengnet_context_message_dispatch(fengnet_monitor *, message_queue *, int weight);	// return next queue
    int fengnet_context_total();
    void fengnet_context_dispatchall(fengnet_context * context);	// for skynet_error output before exit

    void fengnet_context_endless(uint32_t handle);	// for monitor

    void fengnet_profile_enable(int enable);
private:
    void context_inc();
    void context_dec();
    void delete_context(fengnet_context* ctx);
    void dispatch_message(fengnet_context* ctx, fengnet_message* msg);
    // void copy_name(char name[GLOBALNAME_LENGTH], const char* addr);
    static void drop_message(fengnet_message* msg, void* ud);
    // void handle_exit(fengnet_context* context, uint32_t handle);
    // uint32_t tohandle(fengnet_context* context, const char* param);
    // void id_to_hex(char* str, uint32_t id);
private:
    SpinLock lock;
    // fengnet_node G_NODE;
    // shared_ptr<FengnetModule> fengnetModule;
    // shared_ptr<FengnetMQ> fengnetMQ;
    // shared_ptr<FengnetHandle> fengnetHandle;
    // shared_ptr<FengnetTimer> fengnetTimer;
    // shared_ptr<FengnetMonitor> fengnetMonitor;
    // shared_ptr<FengnetLog> fengnetLog;
};

#endif