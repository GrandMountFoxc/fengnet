#ifndef FENGNET_MONITOR_H
#define FENGNET_MONITOR_H

#include "fengnet_malloc.h"
#include "fengnet_server.h"
#include "fengnet.h"

#include <cstdint>
#include <atomic>
#include <cstring>
#include <cstdlib>
#include <memory>

// sknyet_monitor的作用是监视work线程对消息的处理有没有疑似出现死循环。
// 当出现这种情况时，会把目标context endless字段置为true，
// lua层通过skynet.lenless()判断当前处理是否出现死循环。

struct fengnet_monitor {
	atomic<int> version;	// 原子累加的版本号
	int check_version;		// 前一个版本号
	uint32_t source;		// 消息来源
	uint32_t destination;	// 目标
};

class FengnetMonitor{
public:
    static FengnetMonitor* monitorInst;
public:
    FengnetMonitor();
    FengnetMonitor(const FengnetMonitor&) = delete;
    FengnetMonitor& operator=(const FengnetMonitor&) = delete;
    fengnet_monitor* fengnet_monitor_new();
    void fengnet_monitor_delete(fengnet_monitor*);
    void fengnet_monitor_trigger(fengnet_monitor *, uint32_t source, uint32_t destination);
    void fengnet_monitor_check(fengnet_monitor *);
private:
    shared_ptr<FengnetServer> fengnetServer;
};

#endif