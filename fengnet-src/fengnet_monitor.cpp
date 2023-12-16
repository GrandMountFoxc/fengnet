#include "fengnet_monitor.h"

FengnetMonitor* FengnetMonitor::monitorInst;
FengnetMonitor::FengnetMonitor(){
	monitorInst = this;
}

// 新建
fengnet_monitor* FengnetMonitor::fengnet_monitor_new() {
	fengnet_monitor* ret = new fengnet_monitor();
	memset(ret, 0, sizeof(*ret));
	return ret;
}

// 删除
void FengnetMonitor::fengnet_monitor_delete(fengnet_monitor *sm) {
	delete sm;
}

// 赋值来源目标，同时version自增
void FengnetMonitor::fengnet_monitor_trigger(fengnet_monitor *sm, uint32_t source, uint32_t destination) {
	sm->source = source;
	sm->destination = destination;
	sm->version++;
}

// 检查函数
// monitor 的监管逻辑非常简单，每隔 5 s 便为每个 worker 线程执行一次 fengnet_monitor_check 函数
void FengnetMonitor::fengnet_monitor_check(fengnet_monitor* sm) {
	// 当检查到超时处理超时，会把字段endless赋值为0，然后打印一个日志
	// 版本号相同时
	if (sm->version == sm->check_version) {
		// 若目标地址不为 0，则 sm 所对应那个 worker 可能陷入了死循环
		if (sm->destination) {
			fengnetServer->fengnet_context_endless(sm->destination);
			// sm->version是atomic类型，不能直接传入变长参数，这里先用临时变量存一下再传入
			int version = sm->version;
			Fengnet::inst->fengnet_error(NULL, "A message from [ :%08x ] to [ :%08x ] maybe in an endless loop (version = %d)", sm->source , sm->destination, version);
		}
	} else {
		sm->check_version = sm->version;
	}
}