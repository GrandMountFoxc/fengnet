#ifndef FENGNET_HANDLE_H
#define FENGNET_HANDLE_H

// reserve high 8 bits for remote id
#define HANDLE_MASK 0xffffff
#define HANDLE_REMOTE_SHIFT 24

#define DEFAULT_SLOT_SIZE 4
#define MAX_SLOT_SIZE 0x40000000

#include "fengnet_module.h"
#include "fengnet_mq.h"
// #include "fengnet_malloc.h"
#include "fengnet_server.h"

#include <atomic>
#include <shared_mutex>
#include <memory>

struct handle_name {
	char* name;
	uint32_t handle;
};

struct handle_storage {
	mutable std::shared_mutex lock;

	uint32_t harbor;
	uint32_t handle_index;
	int slot_size;
	struct fengnet_context** slot;
	
	int name_cap;
	int name_count;
	struct handle_name* name;	// 一个记录句柄名的有序数组
};

class FengnetHandle{
public:
	static FengnetHandle* handleInst;
public:
	FengnetHandle();
	FengnetHandle(const FengnetHandle&) = delete;
	FengnetHandle& operator=(const FengnetHandle&) = delete;
    uint32_t fengnet_handle_register(fengnet_context*);
    int fengnet_handle_retire(uint32_t handle);
    fengnet_context* fengnet_handle_grab(uint32_t handle);
    void fengnet_handle_retireall();

    uint32_t fengnet_handle_findname(const char* name);
    const char* fengnet_handle_namehandle(uint32_t handle, const char*name);

    void fengnet_handle_init(int harbor);
private:
    handle_storage* H;
    // shared_ptr<FengnetServer> fengnetServer;
private:
	const char* _insert_name(handle_storage* s, const char* name, uint32_t handle);
	void _insert_name_before(handle_storage* s, char* name, uint32_t handle, int before);
};

#endif