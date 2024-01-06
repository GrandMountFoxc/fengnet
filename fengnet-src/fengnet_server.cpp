#include "fengnet_server.h"
#include "fengnet_module.h"
#include "fengnet_handle.h"
#include "fengnet_mq.h"
#include "fengnet_timer.h"
#include "fengnet_harbor.h"
#include "fengnet_env.h"
#include "fengnet_monitor.h"
#include "fengnet_imp.h"
#include "fengnet_log.h"
#include "spinlock_s.h"

#include <thread>

#include <cstring>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdbool>
#include <atomic>

// pthread_key_t 线程的私有存储
// 一个进程中线程直接除了线程自己的栈和寄存器之外，其他几乎都是共享的，如果线程想维护一个只属于线程自己的全局变量就能用上
// https://blog.csdn.net/yusiguyuan/article/details/21785641
// 不论哪个线程调用pthread_key_create()，所创建的key都是所有线程可访问的，但各个线程可根据自己的需要往key中填入不同的值，这就相当于提供了一个同名而不同值的全局变量。
// pthread_key_t handle_key;
//
// C++11 里面提供了thread_local关键字
// 对于线程变量，每个线程都会有该变量的一个拷贝，并行不悖，互不干扰。该局部变量一直都在，直到线程退出为止。这一点和 pthread_key_t 类似
// 系统的线程局部存储区域内存空间并不大，所以尽量不要利用这个空间存储大的数据块，
// 如果不得不使用大的数据块，可以将大的数据块存储在堆内存中，再将该堆内存的地址指针存储在线程局部存储区域。
// thread_local 不能修饰成员变量，所以只能以全局变量的形式存在
thread_local int handle_key;

FengnetServer* FengnetServer::serverInst;
FengnetServer::FengnetServer(){
	serverInst = this;
}

int FengnetServer::fengnet_context_total(){
	return atomic_load(&(Fengnet::inst->G_NODE.total));
	
}

// 注意这里的 atomic<int> 是否线程安全
void FengnetServer::context_inc(){
    Fengnet::inst->G_NODE.total++;
}

// 注意这里的 atomic<int> 是否线程安全
void FengnetServer::context_dec(){
    Fengnet::inst->G_NODE.total--;
}

void FengnetServer::delete_context(fengnet_context* ctx){
    // 保证线程安全情况下加载日志文件
    FILE* f = (FILE*) atomic_load(&ctx->logfile);
    if(f){
        fclose(f);
    }
    FengnetModule::moduleInst->fengnet_module_instance_release(ctx->mod, ctx->instance);
    FengnetMQ::mqInst->fengnet_mq_mark_release(ctx->queue);
    CHECKCALLING_DESTROY(ctx);
    delete ctx;
    context_dec();
}

struct drop_t {
	uint32_t handle;
};

void FengnetServer::drop_message(fengnet_message* msg, void* ud) {
	drop_t *d = static_cast<drop_t*>(ud);
	delete msg->data;
	uint32_t source = d->handle;
	assert(source);
	// report error to the message source
	Fengnet::inst->fengnet_send(nullptr, source, msg->source, PTYPE_ERROR, 0, nullptr, 0);
}

// 加载动态库并初始化
// name 动态库名字
// param 执行动态库中_init函数传入的参数
fengnet_context * FengnetServer::fengnet_context_new(const char * name, const char *param) {
	// 这里说白了就是在加载动态库，先加载了logger.so
	fengnet_module * mod = FengnetModule::moduleInst->fengnet_module_query(name);		// skynet_module.c

	if (mod == NULL)
		return NULL;

	void* inst = FengnetModule::moduleInst->fengnet_module_instance_create(mod);
	if (inst == NULL)
		return NULL;
	// fengnet_context * ctx = fengnet_malloc(sizeof(*ctx));
	fengnet_context * ctx = new fengnet_context();

	// 按照define的逻辑这里是对ctx->calling加锁，但实际上struct skynet_context并没有这个属性
	// 这个定义有一个前提判断也就是 #ifdef CALLING_CHECK
	// 所以推测这里可能就是没有定义CALLING_CHECK，所以没有执行加锁
	CHECKCALLING_INIT(ctx)	

	ctx->mod = mod;
	ctx->instance = inst;
	ctx->ref = 2;
	ctx->cb = NULL;
	ctx->cb_ud = NULL;
	ctx->session_id = 0;
	ctx->logfile = (uintptr_t)NULL;

	ctx->init = false;
	ctx->endless = false;

	ctx->cpu_cost = 0;
	ctx->cpu_start = 0;
	ctx->message_count = 0;
	ctx->profile = Fengnet::inst->G_NODE.profile;
	// Should set to 0 first to avoid skynet_handle_retireall get an uninitialized handle
	ctx->handle = 0;	
	// 把ctx加入到handle里面
	ctx->handle = FengnetHandle::handleInst->fengnet_handle_register(ctx);	// skynet_handle.c
	message_queue * queue = ctx->queue = FengnetMQ::mqInst->fengnet_mq_create(ctx->handle);
	// init function maybe use ctx->handle, so it must init at last
	context_inc();

	CHECKCALLING_BEGIN(ctx)
	// 初始化加载的动态库
	// 按道理说skynet_module_instance_init应该会执行ATOM_FINC(&ctx->ref)，
	// 但暂时没找到，有可能skynet_context_release这里理解的还是有问题
	int r = FengnetModule::moduleInst->fengnet_module_instance_init(mod, inst, ctx, param);		// skynet_module.c
	CHECKCALLING_END(ctx)
	if (r == 0) {	// 初始化成功
		// 这里的释放就显得有些奇怪了
		// 从skynet_context_release函数里面看不是单纯的释放ctx，
		// 反而有点像是在用信号量去计数
		// 其中当ATOM_FDEC(&ctx->ref) == 1成立时才会彻底释放ctx(也就是引用计数为0时释放)
		// 否则会返回ctx
		// 而后续的判断也证明了这一点，只有ret不为NULL时才算启动成功
		fengnet_context * ret = fengnet_context_release(ctx);
		if (ret) {
			ctx->init = true;
		}
		// 这个函数就跟skynet_mq_create里面的注释对应上了
		// message_queue里面有个IN_GLOBAL标志位用来标记是否为全局队列
		// 这里就把刚创建的队列加入全局队列
		// 搞半天全局队列就是之前定义的static struct global_queue *Q = NULL;
		FengnetMQ::mqInst->fengnet_globalmq_push(queue);
		if (ret) {
			// skynet_error按照之前跑出来的结果看应该包含两个部分，一是打印，而是记录日志文件
			Fengnet::inst->fengnet_error(ret, "LAUNCH %s %s", name, param ? param : "");	// skynet_error.c 启动成功
		}
		return ret;
	} else {
		Fengnet::inst->fengnet_error(ctx, "FAILED launch %s", name);
		uint32_t handle = ctx->handle;
		fengnet_context_release(ctx);
		FengnetHandle::handleInst->fengnet_handle_retire(handle);
		drop_t d = { handle };
		FengnetMQ::mqInst->fengnet_mq_release(queue, drop_message, &d);
		return nullptr;
	}
}

// 注意这里的 atomic<int> 是否线程安全
void FengnetServer::fengnet_context_grab(fengnet_context* ctx){
    ctx->ref++;
}

void FengnetServer::fengnet_context_reserve(fengnet_context* ctx){
    fengnet_context_grab(ctx);

    context_dec();
}

// 类似一种变相的判断之前的操作是否执行正确，只有执行正确了才会改变&ctx->ref
// 有点类似shared_ptr，&ctx->ref就是ctx的引用计数
// 只有当ctx-ref减为0时才释放内存
fengnet_context* FengnetServer::fengnet_context_release(fengnet_context* ctx){
    if(ctx->ref.fetch_sub(1)==1){
        delete_context(ctx);
        return nullptr;
    }
    return ctx;
}

uint32_t FengnetServer::fengnet_context_handle(fengnet_context* ctx){
    return ctx->handle;
}

void FengnetServer::fengnet_context_endless(uint32_t handle) {
	fengnet_context * ctx = FengnetHandle::handleInst->fengnet_handle_grab(handle);
	if (ctx == NULL) {
		return;
	}
	ctx->endless = true;
	fengnet_context_release(ctx);
}

void FengnetServer::dispatch_message(fengnet_context* ctx, fengnet_message* msg) {
	assert(ctx->init);
	CHECKCALLING_BEGIN(ctx)
	int type = msg->sz >> MESSAGE_TYPE_SHIFT;
	size_t sz = msg->sz & MESSAGE_TYPE_MASK;
	FILE *f = (FILE *)atomic_load(&ctx->logfile);
	if (f) {
		FengnetLog::fengnet_log_output(f, msg->source, type, msg->session, msg->data, sz);
	}
	++(ctx->message_count);
	int reserve_msg;
	if (ctx->profile) {
		ctx->cpu_start = FengnetTimer::timerInst->fengnet_thread_time();
		// cout<<ctx->handle<<' ';
		reserve_msg = ctx->cb(ctx, ctx->cb_ud, type, msg->session, msg->source, msg->data, sz);
		// cout<<ctx->handle<<endl;
		uint64_t cost_time = FengnetTimer::timerInst->fengnet_thread_time() - ctx->cpu_start;
		ctx->cpu_cost += cost_time;
	} else {
		reserve_msg = ctx->cb(ctx, ctx->cb_ud, type, msg->session, msg->source, msg->data, sz);
	}
	if (!reserve_msg) {
		// fengnet_free(msg->data);
		delete msg->data;
	}
	CHECKCALLING_END(ctx)
}

int FengnetServer::fengnet_context_push(uint32_t handle, fengnet_message* message) {
	fengnet_context* ctx = FengnetHandle::handleInst->fengnet_handle_grab(handle);
	if (ctx == NULL) {
		return -1;
	}
	FengnetMQ::mqInst->fengnet_mq_push(ctx->queue, message);
	fengnet_context_release(ctx);

	return 0;
}

// 将传入的信息加入到ctx消息队列当中
// ctx : skynet_context类型，动态库对应的context
// msg : void*类型，消息主体
// sz : size_t类型，消息长度？
// source : uint32_t类型，消息来源
// type : 
// session : 
void FengnetServer::fengnet_context_send(fengnet_context* ctx, void* msg, size_t sz, uint32_t source, int type, int session) {
	fengnet_message smsg;
	smsg.source = source;
	smsg.session = session;
	smsg.data = msg;
	smsg.sz = sz | (size_t)type << MESSAGE_TYPE_SHIFT;

	FengnetMQ::mqInst->fengnet_mq_push(ctx->queue, &smsg);
}

int FengnetServer::fengnet_context_newsession(fengnet_context* ctx){
	// session always be a positive number
	int session = ++ctx->session_id;
	if (session <= 0) {
		ctx->session_id = 1;
		return 1;
	}
	return session;
}

message_queue* FengnetServer::fengnet_context_message_dispatch(fengnet_monitor* sm, message_queue* q, int weight){
	// 从全局消息队列中取出一个次级消息队列
	if (q == NULL) {
		q = FengnetMQ::mqInst->fengnet_globalmq_pop();
		if (q==NULL)
			return NULL;
	}

	// 获得该次级消息队列所对应的服务的句柄
	uint32_t handle = FengnetMQ::mqInst->fengnet_mq_handle(q);

	// 获取服务上下文
	struct fengnet_context * ctx = FengnetHandle::handleInst->fengnet_handle_grab(handle);
	// 若取出的服务没有上下文，则重取一个新的次级消息队列
	if (ctx == NULL) {
		struct drop_t d = { handle };
		FengnetMQ::mqInst->fengnet_mq_release(q, drop_message, &d);
		return FengnetMQ::mqInst->fengnet_globalmq_pop();
	}

	int i,n=1;
	struct fengnet_message msg;

	// 根据不同的权重从消息队列中获得不同数量的消息
	for (i=0;i<n;i++) {
		if (FengnetMQ::mqInst->fengnet_mq_pop(q,&msg)) {
			fengnet_context_release(ctx);
			return FengnetMQ::mqInst->fengnet_globalmq_pop();
		} else if (i==0 && weight >= 0) {
			n = FengnetMQ::mqInst->fengnet_mq_length(q);
			n >>= weight;
		}
		int overload = FengnetMQ::mqInst->fengnet_mq_overload(q);
		if (overload) {
			Fengnet::inst->fengnet_error(ctx, "May overload, message queue length = %d", overload);
		}

		// 调用消息处理回调时，先设置好检查的来源handle和目标handle，消息处理完了清除来源和目标信息。
		// 过程中version会累加2次。
		FengnetMonitor::monitorInst->fengnet_monitor_trigger(sm, msg.source , handle);

		if (ctx->cb == NULL) {
			// fengnet_free(msg.data);
			delete msg.data;
		} else {
			dispatch_message(ctx, &msg);
		}
		assert(handle==ctx->handle);
		
		// 清除目标和来源保证 monitor 不会误判出现死循环
		FengnetMonitor::monitorInst->fengnet_monitor_trigger(sm, 0,0);
	}

	assert(q == ctx->queue);
	message_queue* nq = FengnetMQ::mqInst->fengnet_globalmq_pop();
	if (nq) {
		// If global mq is not empty , push q back, and return next queue (nq)
		// Else (global mq is empty or block, don't push q back, and return q again (for next dispatch)
		FengnetMQ::mqInst->fengnet_globalmq_push(q);
		q = nq;
	} 
	fengnet_context_release(ctx);

	return q;
}

void FengnetServer::fengnet_context_dispatchall(fengnet_context* ctx) {
	// for skynet_error
	fengnet_message msg;
	message_queue* q = ctx->queue;
	while (!FengnetMQ::mqInst->fengnet_mq_pop(q,&msg)) {
		dispatch_message(ctx, &msg);
	}
}

void FengnetServer::fengnet_globalinit(void){
    Fengnet::inst->G_NODE.total = 0;
    Fengnet::inst->G_NODE.monitor_exit = 0;
    Fengnet::inst->G_NODE.init = 1;

	// fengnetModule = make_shared<FengnetModule>();
	// fengnetMQ = make_shared<FengnetMQ>();
	// fengnetHandle = make_shared<FengnetHandle>();

    // C++11 不需要这个判断，因为thread_local不需要创建
    // if (pthread_key_create(&G_NODE.handle_key, NULL)) {
	// 	fprintf(stderr, "pthread_key_create failed");
	// 	exit(1);
	// }

    // set mainthread's key
    fengnet_initthread(THREAD_MAIN);
}

void FengnetServer::fengnet_globalexit(void){
    // 释放线程局部存储，但 thread_local 好像没这个步骤
    // pthread_key_delete(G_NODE.handle_key);
}

// 给handle_key设置初始值
void FengnetServer::fengnet_initthread(int m){
    uintptr_t v = (uint32_t)(-m);
    // 这里的 v 类型可能不对
    handle_key = v;
}

void FengnetServer::fengnet_profile_enable(int enable){
    Fengnet::inst->G_NODE.profile = (bool)enable;
}