#include "fengnet_mq.h"

#include <thread>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdbool>

FengnetMQ* FengnetMQ::mqInst;
FengnetMQ::FengnetMQ(){
	mqInst = this;
}

void FengnetMQ::fengnet_globalmq_push(message_queue* queue){
    global_queue* q = Q;

    lock_guard<SpinLock> lock(q->lock);
    assert(queue->next==nullptr);
    if(q->tail){
        q->tail->next = queue;
        q->tail = queue;
    }else{
        q->head = q->tail = queue;
    }
}


message_queue* FengnetMQ::fengnet_globalmq_pop(){
    global_queue *q = Q;

	lock_guard<SpinLock> lock(q->lock);
	message_queue *mq = q->head;
	if(mq) {
		q->head = mq->next;
		if(q->head == nullptr) {
			assert(mq == q->tail);
			q->tail = nullptr;
		}
		mq->next = nullptr;
	}

	return mq;
}

message_queue* FengnetMQ::fengnet_mq_create(uint32_t handle){
    message_queue *q = new message_queue();
	q->handle = handle;
	q->cap = DEFAULT_QUEUE_SIZE;
	q->head = 0;
	q->tail = 0;
	// When the queue is create (always between service create and service init) ,
	// set in_global flag to avoid push it to global queue .
	// If the service init success, skynet_context_new will call skynet_mq_push to push it to global queue.
	// 这一段是想表达用in_global标签去标记这个消息队列是否是全局的，但具体作用感觉不是很清楚
	q->in_global = MQ_IN_GLOBAL;
	q->release = 0;
	q->overload = 0;
	q->overload_threshold = MQ_OVERLOAD;
	// q->queue = fengnet_malloc(sizeof(fengnet_message) * q->cap);
	q->queue = new fengnet_message[q->cap]();
	q->next = nullptr;

	return q;
}

void FengnetMQ::_release(message_queue* q) {
	assert(q->next == NULL);
	// fengnet_free(q->queue);
	delete[] q->queue;
	// fengnet_free(q);
	delete q;
}

void FengnetMQ::fengnet_mq_mark_release(message_queue *q){
    lock_guard<SpinLock> lock(q->lock);
    assert(q->release==0);
    q->release = 1;
    if(q->in_global != MQ_IN_GLOBAL){
        fengnet_globalmq_push(q);
    }
}

int FengnetMQ::fengnet_mq_pop(message_queue* q, fengnet_message* message){
    int ret = 1;
	lock_guard<SpinLock> lock(q->lock);

	if (q->head != q->tail) {
		*message = q->queue[q->head++];
		ret = 0;
		int head = q->head;
		int tail = q->tail;
		int cap = q->cap;

		if (head >= cap) {
			q->head = head = 0;
		}
		int length = tail - head;
		if (length < 0) {
			length += cap;
		}
		while (length > q->overload_threshold) {
			q->overload = length;
			q->overload_threshold *= 2;
		}
	} else {
		// reset overload_threshold when queue is empty
		q->overload_threshold = MQ_OVERLOAD;
	}

	if (ret) {
		q->in_global = 0;
	}

	return ret;
}

void FengnetMQ::expand_queue(message_queue* q) {
	// fengnet_message *new_queue = fengnet_malloc(sizeof(fengnet_message) * q->cap * 2);
	fengnet_message *new_queue = new fengnet_message[q->cap * 2]();
	int i;
	for (i=0;i<q->cap;i++) {
		new_queue[i] = q->queue[(q->head + i) % q->cap];
	}
	q->head = 0;
	q->tail = q->cap;
	q->cap *= 2;
	
	// fengnet_free(q->queue);
	delete [] q->queue;
	q->queue = new_queue;
}

void FengnetMQ::fengnet_mq_push(message_queue* q, fengnet_message* message){
    assert(message);
	lock_guard<SpinLock> lock(q->lock);

	q->queue[q->tail] = *message;
	if (++ q->tail >= q->cap) {
		q->tail = 0;
	}

	if (q->head == q->tail) {
		expand_queue(q);
	}

	if (q->in_global == 0) {
		q->in_global = MQ_IN_GLOBAL;
		fengnet_globalmq_push(q);
	}
	
}

uint32_t FengnetMQ::fengnet_mq_handle(message_queue* q){
    return q->handle;
}

int FengnetMQ::fengnet_mq_length(message_queue* q){
    int head, tail,cap;

	unique_lock<SpinLock> lock(q->lock);
	head = q->head;
	tail = q->tail;
	cap = q->cap;
	lock.unlock();
	
	if (head <= tail) {
		return tail - head;
	}
	return tail + cap - head;
}

int FengnetMQ::fengnet_mq_overload(message_queue* q){
    if (q->overload) {
		int overload = q->overload;
		q->overload = 0;
		return overload;
	} 
	return 0;
}

void FengnetMQ::_drop_queue(message_queue* q, message_drop drop_func, void* ud) {
	struct fengnet_message msg;
	while(!fengnet_mq_pop(q, &msg)) {
		drop_func(&msg, ud);
	}
	_release(q);
}

void FengnetMQ::fengnet_mq_release(struct message_queue *q, message_drop drop_func, void *ud) {
	unique_lock<SpinLock> lock(q->lock);
	
	if (q->release) {
		lock.unlock();
		_drop_queue(q, drop_func, ud);
	} else {
		fengnet_globalmq_push(q);
		lock.unlock();
	}
}

void FengnetMQ::fengnet_mq_init(){
    global_queue* q = new global_queue();
    memset(q, 0, sizeof(*q));
    Q = q;
}