#include "fengnet_timer.h"

FengnetTimer* FengnetTimer::timerInst;
FengnetTimer::FengnetTimer(){
	timerInst = this;
	timerInst->TI = nullptr;
}

// link_clear()清空链表，头尾节点指向同一块内存
timer_node* FengnetTimer::link_clear(link_list* list){
    timer_node* ret = list->head.next;
    list->head.next = 0;
    list->tail = &(list->head);

    return ret;
}

void FengnetTimer::link(link_list* list, timer_node* node) {
	list->tail->next = node;
	list->tail = node;
	node->next=0;
}

void FengnetTimer::add_node(timer* T, timer_node* node) {
	uint32_t time=node->expire;
	uint32_t current_time=T->time;
	
	if ((time|TIME_NEAR_MASK)==(current_time|TIME_NEAR_MASK)) {
		link(&T->near[time&TIME_NEAR_MASK],node);
	} else {
		int i;
		uint32_t mask=TIME_NEAR << TIME_LEVEL_SHIFT;
		for (i=0;i<3;i++) {
			if ((time|(mask-1))==(current_time|(mask-1))) {
				break;
			}
			mask <<= TIME_LEVEL_SHIFT;
		}

		link(&T->t[i][((time>>(TIME_NEAR_SHIFT + i*TIME_LEVEL_SHIFT)) & TIME_LEVEL_MASK)],node);	
	}
}

void FengnetTimer::timer_add(timer* T, void* arg, size_t sz, int time) {
	// timer_node* node = (struct timer_node *)skynet_malloc(sizeof(*node)+sz);
	timer_node* node = (timer_node *)malloc(sizeof(*node)+sz);
	memcpy(node+1,arg,sz);

	lock_guard<SpinLock> lock(T->lock);

	node->expire=time+T->time;
	add_node(T, node);
}

void FengnetTimer::move_list(timer* T, int level, int idx) {
	timer_node* current = link_clear(&T->t[level][idx]);
	while (current) {
		timer_node* temp=current->next;
		add_node(T,current);
		current=temp;
	}
}

void FengnetTimer::timer_shift(timer* T) {
	int mask = TIME_NEAR;
	uint32_t ct = ++T->time;
	if (ct == 0) {
		move_list(T, 3, 0);
	} else {
		uint32_t time = ct >> TIME_NEAR_SHIFT;
		int i=0;

		while ((ct & (mask-1))==0) {
			int idx=time & TIME_LEVEL_MASK;
			if (idx!=0) {
				move_list(T, i, idx);
				break;				
			}
			mask <<= TIME_LEVEL_SHIFT;
			time >>= TIME_LEVEL_SHIFT;
			++i;
		}
	}
}

void FengnetTimer::dispatch_list(timer_node* current) {
	do {
		timer_event* event = (timer_event*)(current+1);
		fengnet_message message;
		message.source = 0;
		message.session = event->session;
		message.data = NULL;
		message.sz = (size_t)PTYPE_RESPONSE << MESSAGE_TYPE_SHIFT;

		FengnetServer::serverInst->fengnet_context_push(event->handle, &message);
		
		timer_node* temp = current;
		current=current->next;
		delete temp;	
	} while (current);
}

void FengnetTimer::timer_execute(timer* T) {
	int idx = T->time & TIME_NEAR_MASK;
	
	while (T->near[idx].head.next) {
		timer_node* current = link_clear(&T->near[idx]);
		unique_lock<SpinLock> lock(T->lock);
		// dispatch_list don't need lock T
		dispatch_list(current);
		lock.unlock();
	}
}

void FengnetTimer::timer_update(timer* T) {
	lock_guard<SpinLock> lock(T->lock);

	// try to dispatch timeout 0 (rare condition)
	timer_execute(T);

	// shift time first, and then dispatch timer message
	timer_shift(T);

	timer_execute(T);
}

int FengnetTimer::fengnet_timeout(uint32_t handle, int time, int session) {
	if (time <= 0) {
		fengnet_message message;
		message.source = 0;
		message.session = session;
		message.data = NULL;
		message.sz = (size_t)PTYPE_RESPONSE << MESSAGE_TYPE_SHIFT;

		if (FengnetServer::serverInst->fengnet_context_push(handle, &message)) {
			return -1;
		}
	} else {
		timer_event event;
		event.handle = handle;
		event.session = session;
		timer_add(TI, &event, sizeof(event), time);
	}

	return session;
}

// centisecond: 1/100 second
// 获取系统当前时间
void FengnetTimer::systime(uint32_t *sec, uint32_t *cs) {
	// struct timespec有两个成员，一个是秒，一个是纳秒
	// struct timespec {
	// 		time_t tv_sec; // seconds 代表了秒级的时间
	// 		long tv_nsec; // and nanoseconds 代表了纳秒级的时间
	// };
	// tv_sec和tv_nsec相加才是最终消耗的时间，例如，一个struct timespec结构的值为{10, 500000000}表示了10秒加上500000000纳秒，即10.5秒。
	struct timespec ti;		
	// int clock_gettime(clockid_t clk_id,struct timespec *tp);
	// clk_id : 检索和设置的clk_id指定的时钟时间。
	// CLOCK_REALTIME:系统实时时间,随系统实时时间改变而改变,即从UTC1970-1-1 0:0:0开始计时,中间时刻如果系统时间被用户改成其他,则对应的时间相应改变。
	// CLOCK_MONOTONIC:从系统启动这一刻起开始计时,不受系统时间被用户改变的影响。
	// CLOCK_PROCESS_CPUTIME_ID:本进程到当前代码系统CPU花费的时间。
	// CLOCK_THREAD_CPUTIME_ID:本线程到当前代码系统CPU花费的时间。
	clock_gettime(CLOCK_REALTIME, &ti);
	*sec = (uint32_t)ti.tv_sec;
	*cs = (uint32_t)(ti.tv_nsec / 10000000);	// nanosecond/10000000->centisecond
}

// 获取系统启动时间 CLOCK_MONOTONIC
uint64_t FengnetTimer::gettime() {
	uint64_t t;
	struct timespec ti;
	clock_gettime(CLOCK_MONOTONIC, &ti);	// 获取系统启动时间作为计时器的current_point，应该会用在后续程序中需要测量时间或延迟的场景。
	t = (uint64_t)ti.tv_sec * 100;
	t += ti.tv_nsec / 10000000;
	return t;
}

void FengnetTimer::fengnet_updatetime() {
	uint64_t cp = gettime();
	if(cp < TI->current_point) {
		Fengnet::inst->fengnet_error(NULL, "time diff error: change from %lld to %lld", cp, TI->current_point);
		TI->current_point = cp;
	} else if (cp != TI->current_point) {
		uint32_t diff = (uint32_t)(cp - TI->current_point);
		TI->current_point = cp;
		TI->current += diff;
		int i;
		for (i=0;i<diff;i++) {
			timer_update(TI);
		}
	}
}

uint32_t FengnetTimer::fengnet_starttime() {
	return TI->starttime;
}

uint64_t FengnetTimer::fengnet_now(){
	return TI->current;
}

timer* FengnetTimer::timer_create_timer(){
    timer* r = new timer();
    memset(r, 0, sizeof(*r));

    int i, j;

    for(i=0;i<TIME_NEAR;++i){
        link_clear(&r->near[i]);
    }

    for(i=0;i<4;++i){
        for(j=0;j<TIME_LEVEL;++j){
            link_clear(&r->t[i][j]);
        }
    }

    r->current = 0;

    return r;
}

void FengnetTimer::fengnet_timer_init(){
    TI = timer_create_timer();
    uint32_t current = 0;
    systime(&TI->starttime, &current);
    TI->current = current;
    TI->current_point = gettime();
	// fengnetServer = make_shared<FengnetServer>();
}

// for profile

#define NANOSEC 1000000000
#define MICROSEC 1000000

uint64_t FengnetTimer::fengnet_thread_time(void) {
	struct timespec ti;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ti);

	return (uint64_t)ti.tv_sec * MICROSEC + (uint64_t)ti.tv_nsec / (NANOSEC / MICROSEC);
}