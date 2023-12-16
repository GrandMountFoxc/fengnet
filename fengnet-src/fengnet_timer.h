#ifndef FENGNET_TIMER_H
#define FENGNET_TIMER_H

#include <cstdint>
#include <ctime>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <mutex>

#include "spinlock.h"
#include "fengnet_malloc.h"
#include "fengnet_server.h"
#include "fengnet_mq.h"

typedef void (*timer_execute_func)(void *ud,void *arg);

#define TIME_NEAR_SHIFT 8
#define TIME_NEAR (1 << TIME_NEAR_SHIFT)
#define TIME_LEVEL_SHIFT 6
#define TIME_LEVEL (1 << TIME_LEVEL_SHIFT)
#define TIME_NEAR_MASK (TIME_NEAR-1)
#define TIME_LEVEL_MASK (TIME_LEVEL-1)

struct timer_event {
	uint32_t handle;
	int session;
};

struct timer_node {
	timer_node *next;
	uint32_t expire;
};

struct link_list {
	timer_node head;
	timer_node *tail;
};

struct timer {
	link_list near[TIME_NEAR];
	link_list t[4][TIME_LEVEL];
	SpinLock lock;
	uint32_t time;
	uint32_t starttime;
	uint64_t current;
	uint64_t current_point;
};

class FengnetTimer{
public:
	static FengnetTimer* timerInst;
	FengnetTimer();
	FengnetTimer(const FengnetTimer&) = delete;
	FengnetTimer& operator=(const FengnetTimer&) = delete;
    int fengnet_timeout(uint32_t handle, int time, int session);
    void fengnet_updatetime();
    uint32_t fengnet_starttime();
    uint64_t fengnet_thread_time();	// for profile, in micro second
	uint64_t fengnet_now();

    void fengnet_timer_init();
private:
    timer* TI;
	// shared_ptr<FengnetServer> fengnetServer;
private:
    timer* timer_create_timer();
    inline timer_node* link_clear(link_list* list);
	inline void link(link_list* list, timer_node* node);
    void systime(uint32_t *sec, uint32_t *cs);
    uint64_t gettime();
	void timer_add(timer *T,void *arg,size_t sz,int time);
	void add_node(timer* T, timer_node* node);
	void move_list(timer* T, int level, int idx);
	void timer_shift(timer* T);
	inline void dispatch_list(timer_node* current);
	inline void timer_execute(timer* T);
	void timer_update(timer* T);
};

#endif
