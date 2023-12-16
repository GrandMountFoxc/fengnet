#ifndef FENGNET_IMP_H
#define FENGNET_IMP_H

#include <string>

struct fengnet_config {
	int thread;
	int harbor;
	int profile;
	const char* daemon;
	const char* module_path;
	const char* bootstrap;
	const char* logger;
	const char* logservice;
};

// 总共5个线程
#define THREAD_WORKER 0
#define THREAD_MAIN 1
#define THREAD_SOCKET 2
#define THREAD_TIMER 3
#define THREAD_MONITOR 4

void fengnet_start(fengnet_config* config);

#endif