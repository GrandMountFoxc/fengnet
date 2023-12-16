#ifndef FENGNET_MQ_H
#define FENGNET_MQ_H

#include <cstdio>
#include <cstdint>
#include <mutex>

#include "spinlock.h"

struct fengnet_message {
	uint32_t source;
	int session;
	void* data;
	size_t sz;
};

// type is encoding in skynet_message.sz high 8bit
#define MESSAGE_TYPE_MASK (SIZE_MAX >> 8)
#define MESSAGE_TYPE_SHIFT ((sizeof(size_t)-1) * 8)

#define DEFAULT_QUEUE_SIZE 64
#define MAX_GLOBAL_MQ 0x10000

// 0 means mq is not in global mq.
// 1 means mq is in global mq , or the message is dispatching.

#define MQ_IN_GLOBAL 1
#define MQ_OVERLOAD 1024

typedef void (*message_drop)(fengnet_message *, void *);

struct message_queue {
	SpinLock lock;      // 自旋锁是否有问题
	uint32_t handle;
	int cap;
	int head;
	int tail;
	int release;
	int in_global;
	int overload;
	int overload_threshold;
	fengnet_message *queue;
	message_queue *next;
};

struct global_queue {
	message_queue *head;
	message_queue *tail;
	SpinLock lock;      // 自旋锁是否有问题
};

class FengnetMQ{
public:
	static FengnetMQ* mqInst;
public:
	FengnetMQ();
	FengnetMQ(const FengnetMQ&) = delete;
	FengnetMQ& operator=(const FengnetMQ&) = delete;
    void fengnet_globalmq_push(message_queue * queue);
    message_queue * fengnet_globalmq_pop(void);

    message_queue * fengnet_mq_create(uint32_t handle);
    void fengnet_mq_mark_release(message_queue *q);

    void fengnet_mq_release(message_queue *q, message_drop drop_func, void *ud);
    uint32_t fengnet_mq_handle(message_queue *);

    // 0 for success
    int fengnet_mq_pop(message_queue *q, fengnet_message *message);
    void fengnet_mq_push(message_queue *q, fengnet_message *message);

    // return the length of message queue, for debug
    int fengnet_mq_length(message_queue *q);
    int fengnet_mq_overload(message_queue *q);

    void fengnet_mq_init();
private:
    global_queue* Q;
private:
	void expand_queue(struct message_queue* q);
	void _drop_queue(message_queue* q, message_drop drop_func, void* ud);
	void _release(struct message_queue *q);
};

#endif