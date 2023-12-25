#include "fengnet.h"
#include "fengnet_server.h"
#include "fengnet_imp.h"
#include "fengnet_mq.h"
#include "fengnet_handle.h"
#include "fengnet_module.h"
#include "fengnet_timer.h"
#include "fengnet_monitor.h"
#include "fengnet_socket.h"
#include "fengnet_daemon.h"
#include "fengnet_harbor.h"

#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>

using namespace std;

struct monitor {
	int count;					// monitor 所监视的 worker 线程的数量
	fengnet_monitor** m;	// 存放所有的 skynet_monitor 的数组，worker 和 skynet_monitor 一一对应
	condition_variable cond;
	mutex mtx;
	int sleep;					// 休眠时间
	int quit;					// 退出标志
};

struct worker_parm {
	monitor* m;
	int id;
	int weight;
};


static volatile int SIG = 0;


static auto fengnetDaemon = make_shared<FengnetDaemon>();
// static auto fengnetHarbor = make_shared<FengnetHarbor>();
// static auto fengnetMQ = make_shared<FengnetMQ>();
// static auto fengnetModule = make_shared<FengnetModule>();
// static auto fengnetServer = make_shared<FengnetServer>();
// static auto fengnetHandle = make_shared<FengnetHandle>();

#define CHECK_ABORT if (FengnetServer::serverInst->fengnet_context_total()==0) break;

// 
static void handle_hup(int signal) {
	if (signal == SIGHUP) {
		SIG = 1;
	}
}

static void wakeup(monitor* m, int busy) {
	if (m->sleep >= m->count - busy) {
		// signal sleep worker, "spurious wakeup" is harmless
		m->cond.notify_one();
	}
}

// static void create_thread(thread thd, void *(*start_routine) (void *), void *arg) {
// 	thd = thread(start_routine, arg); 
// 	if (pthread_create(thread,NULL, start_routine, arg)) {
// 		fprintf(stderr, "Create thread failed");
// 		exit(1);
// 	}
// }

static void* thread_socket(void *p) {
	monitor* m = static_cast<monitor*>(p);
	FengnetServer::serverInst->fengnet_initthread(THREAD_SOCKET);
	for (;;) {
		int r = FengnetSocket::socketInst->fengnet_socket_poll();
		if (r==0)
			break;
		if (r<0) {
			CHECK_ABORT
			continue;
		}
		wakeup(m,0);
	}
	return nullptr;
}

static void free_monitor(monitor* m) {
	int i;
	int n = m->count;
	for (i=0;i<n;i++) {
		FengnetMonitor::monitorInst->fengnet_monitor_delete(m->m[i]);
	}
	// fengnet_free(m->m);
	delete []m->m;
	// fengnet_free(m);
	delete m;
}

// 每5秒对所以worker线程进行一次检查调用
static void* thread_monitor(void *p) {
	monitor * m = static_cast<monitor*>(p);
	int i;
	int n = m->count;
	FengnetServer::serverInst->fengnet_initthread(THREAD_MONITOR);
	for (;;) {
		CHECK_ABORT
		for (i=0;i<n;i++) {
			FengnetMonitor::monitorInst->fengnet_monitor_check(m->m[i]);
		}
		for (i=0;i<5;i++) {
			CHECK_ABORT
			sleep(1);
		}
	}

	return nullptr;
}

static void signal_hup() {
	// make log file reopen

	fengnet_message smsg;
	smsg.source = 0;
	smsg.session = 0;
	smsg.data = NULL;
	smsg.sz = (size_t)PTYPE_SYSTEM << MESSAGE_TYPE_SHIFT;
	uint32_t logger = FengnetHandle::handleInst->fengnet_handle_findname("logger");
	if (logger) {
		FengnetServer::serverInst->fengnet_context_push(logger, &smsg);
	}
}

static void* thread_timer(void *p) {
	monitor* m = static_cast<monitor*>(p);
	FengnetServer::serverInst->fengnet_initthread(THREAD_TIMER);
	for (;;) {
		FengnetTimer::timerInst->fengnet_updatetime();
		FengnetSocket::socketInst->fengnet_socket_updatetime();
		CHECK_ABORT
		wakeup(m,m->count-1);
		usleep(2500);
		if (SIG) {
			signal_hup();
			SIG = 0;
		}
	}
	// wakeup socket thread
	FengnetSocket::socketInst->fengnet_socket_exit();
	// wakeup all worker thread
	lock_guard<mutex> lock(m->mtx);
	m->quit = 1;
	m->cond.notify_all();

	return nullptr;
}

static void* thread_worker(void *p) {
	worker_parm *wp = static_cast<worker_parm*>(p);
	int id = wp->id;
	int weight = wp->weight;
	monitor *m = wp->m;
	fengnet_monitor *sm = m->m[id];
	FengnetServer::serverInst->fengnet_initthread(THREAD_WORKER);
	message_queue * q = NULL;
	while (!m->quit) {
		q = FengnetServer::serverInst->fengnet_context_message_dispatch(sm, q, weight);
		if (q == NULL) {
			unique_lock<mutex> lock(m->mtx);
			++ m->sleep;
			// "spurious wakeup" is harmless,
			// because skynet_context_message_dispatch() can be call at any time.
			if (!m->quit)
				m->cond.wait(lock);
			-- m->sleep;
			lock.unlock();
			// if (pthread_mutex_unlock(&m->mutex)) {
			// 	fprintf(stderr, "unlock mutex error");
			// 	exit(1);
			// }
		}
	}
	return nullptr;
}

static void bootstrap(fengnet_context* logger, const char* cmdline) {
	int sz = strlen(cmdline);
	char name[sz+1];
	char args[sz+1];
	int arg_pos;
	// int sscanf(const char *str, const char *format, ...)
	// 从字符串读取格式化输入
	// %s 字符串。这将读取连续字符，直到遇到一个空格字符（空格字符可以是空白、换行和制表符）
	// cmdline = "snlua bootstrap" -> name = {"snlua", "bootstrap"}
	sscanf(cmdline, "%s", name);  
	arg_pos = strlen(name);
	if (arg_pos < sz) {
		while(cmdline[arg_pos] == ' ') {
			arg_pos++;
		}
		strncpy(args, cmdline + arg_pos, sz);
	} else {
		args[0] = '\0';
	}
	// 加载snlua动态库并初始化
	struct fengnet_context *ctx = FengnetServer::serverInst->fengnet_context_new(name, args);
	if (ctx == nullptr) {
		Fengnet::inst->fengnet_error(nullptr, "Bootstrap error : %s\n", cmdline);
		FengnetServer::serverInst->fengnet_context_dispatchall(logger);
		exit(1);
	}
}

static void
start(int thread_num) {
	thread pid[thread_num+3];

	// monitor *m = fengnet_malloc(sizeof(*m));
	monitor* m = new monitor();
	memset(m, 0, sizeof(*m));
	m->count = thread_num;
	m->sleep = 0;

	// m->m = fengnet_malloc(thread_num * sizeof(fengnet_monitor*));
	m->m = new fengnet_monitor*[thread_num]();
	int i;
	for (i=0;i<thread_num;i++) {
		m->m[i] = FengnetMonitor::monitorInst->fengnet_monitor_new();
	}

	// create_thread(&pid[0], thread_monitor, m);
	// create_thread(&pid[1], thread_timer, m);
	// create_thread(&pid[2], thread_socket, m);
	// 还是应该封装一下，防止创建线程失败
	pid[0] = thread(thread_monitor, m);
	pid[1] = thread(thread_timer, m);
	pid[2] = thread(thread_socket, m);

	// weight是对应的线程的权重，当有8个线程时，线程的权重为-1，-1，-1，-1，0，0，0，0
	// 当权重为0，该线线程会处理全部消息，n = len >> weight = len / 2^weight
	// 当权重为1，该线线程会一半消息，n = len >> weight = len / 2^weight
	// 当权重为2，该线线程会4分之一消息，n = len >> weight = len / 2^weight
	// 当权重为3，该线线程会8分之一消息，n = len >> weight = len / 2^weight
	// 这种分配优先级的做法，使得 CPU 的运转效率尽可能的高。
	// 当线程足够多时，如果每次都只处理一个消息，虽然可以避免一些服务饿死，但却可能会使得消息队列中出现大量消息堆积。
	// 如果每次都处理一整个消息队列中的消息，则可能会使一些服务中的消息长时间得不到相应，从而导致服务饿死。为线程配置权重的做法是一个非常好的折中方案
	static int weight[] = { 
		-1, -1, -1, -1, 0, 0, 0, 0,
		1, 1, 1, 1, 1, 1, 1, 1, 
		2, 2, 2, 2, 2, 2, 2, 2, 
		3, 3, 3, 3, 3, 3, 3, 3, };
	worker_parm wp[thread_num];
	for (i=0;i<thread_num;i++) {
		wp[i].m = m;
		wp[i].id = i;
		if (i < sizeof(weight)/sizeof(weight[0])) {
			wp[i].weight= weight[i];
		} else {
			wp[i].weight = 0;
		}
		// create_thread(&pid[i+3], thread_worker, &wp[i]);
		pid[i+3] = thread(thread_worker, &wp[i]);
	}

	for (i=0;i<thread_num+3;i++) {
		pid[i].join(); 
	}

	free_monitor(m);
}

void fengnet_start(fengnet_config* config){
    // struct sigaction结构体
	// struct sigaction {
	// 		void (*sa_handler)(int);	// 对捕获的信号进行处理的函数，函数参数为sigaction函数的参数1信号（概念上等同于单独使用signal函数）
	// 		void (*sa_sigaction)(int, siginfo_t *, void *);
	// 		sigset_t sa_mask;
	// 		int sa_flags;				// 指定了对信号进行哪些特殊的处理
	// 		void (*sa_restorer)(void);
	// };
	// register SIGHUP for log file reopen
	struct sigaction sa;
    sa.sa_handler = &handle_hup;	// 这个函数没懂干嘛的
	sa.sa_flags = SA_RESTART;	    // 由此信号中断的系统调用自动重启动
	sigfillset(&sa.sa_mask);
	sigaction(SIGHUP, &sa, NULL);

    if(config->daemon){
        if(fengnetDaemon->daemon_init(config->daemon)){
            exit(1);
        }
    }
    new FengnetTimer();
	new FengnetSocket();
	new FengnetMonitor();
	new FengnetModule();
	new FengnetMQ();
	new FengnetHandle();
	new FengnetHarbor();
	FengnetHarbor::harborInst->fengnet_harbor_init(config->harbor);
	FengnetHandle::handleInst->fengnet_handle_init(config->harbor);
	FengnetMQ::mqInst->fengnet_mq_init();
	FengnetModule::moduleInst->fengnet_module_init(config->module_path);
	FengnetTimer::timerInst->fengnet_timer_init();
	FengnetSocket::socketInst->fengnet_socket_init();
	FengnetServer::serverInst->fengnet_profile_enable(config->profile);

	// 加载logger动态库并初始化
	fengnet_context* ctx = FengnetServer::serverInst->fengnet_context_new(config->logservice, config->logger);
	if (ctx == nullptr) {
		fprintf(stderr, "Can't launch %s service\n", config->logservice);
		exit(1);
	}

	// 把"logger"加入到handle里面
	FengnetHandle::handleInst->fengnet_handle_namehandle(FengnetServer::serverInst->fengnet_context_handle(ctx), "logger");

	bootstrap(ctx, config->bootstrap);

	start(config->thread);

}