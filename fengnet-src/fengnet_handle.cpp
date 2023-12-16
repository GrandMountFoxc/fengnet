#include "fengnet_handle.h"

FengnetHandle* FengnetHandle::handleInst;
FengnetHandle::FengnetHandle(){
	handleInst = this;
}

uint32_t FengnetHandle::fengnet_handle_register(fengnet_context* ctx) {
	struct handle_storage *s = H;

	// rwlock_wlock(&s->lock);
    std::unique_lock<std::shared_mutex> lock(s->lock);
	
	for (;;) {
		int i;
		uint32_t handle = s->handle_index;
		// s->slot_size默认设为4，而且下标0是给system的，所以s->slot只能存3个值
		for (i=0;i<s->slot_size;i++,handle++) {
			if (handle > HANDLE_MASK) {
				// 0 is reserved
				handle = 1;
			}
			int hash = handle & (s->slot_size-1);
			if (s->slot[hash] == NULL) {
				s->slot[hash] = ctx;
				s->handle_index = handle + 1;

				// rwlock_wunlock(&s->lock);
                lock.unlock();

				handle |= s->harbor;
				return handle;
			}
		}
		// 下面其实就是针对上面的疑问进行了解决，也就是动态扩容（2倍）
		assert((s->slot_size*2 - 1) <= HANDLE_MASK);
		// fengnet_context ** new_slot = fengnet_malloc(s->slot_size * 2 * sizeof(fengnet_context *));
		fengnet_context** new_slot = new fengnet_context*[s->slot_size * 2]();
		memset(new_slot, 0, s->slot_size * 2 * sizeof(fengnet_context *));
		for (i=0;i<s->slot_size;i++) {
			if (s->slot[i]) {
				int hash = FengnetServer::serverInst->fengnet_context_handle(s->slot[i]) & (s->slot_size * 2 - 1);
				assert(new_slot[hash] == NULL);
				new_slot[hash] = s->slot[i];
			}
		}
		delete[] s->slot;
		s->slot = new_slot;
		s->slot_size *= 2;
	}
}

void FengnetHandle::fengnet_handle_retireall() {
	handle_storage *s = H;
	for (;;) {
		int n=0;
		int i;
		for (i=0;i<s->slot_size;i++) {
			std::shared_lock<std::shared_mutex> lock(s->lock);
			fengnet_context * ctx = s->slot[i];
			uint32_t handle = 0;
			if (ctx) {
				handle = FengnetServer::serverInst->fengnet_context_handle(ctx);
				++n;
			}
			// rwlock_runlock(&s->lock);

			if (handle != 0) {
				fengnet_handle_retire(handle);
			}
		}
		if (n==0)
			return;
	}
}

int FengnetHandle::fengnet_handle_retire(uint32_t handle){
    int ret = 0;
	handle_storage *s = H;

	std::unique_lock<std::shared_mutex> lock(s->lock);

	uint32_t hash = handle & (s->slot_size-1);
	fengnet_context * ctx = s->slot[hash];

	if (ctx != NULL && FengnetServer::serverInst->fengnet_context_handle(ctx) == handle) {
		s->slot[hash] = NULL;
		ret = 1;
		int i;
		int j=0, n=s->name_count;
		for (i=0; i<n; ++i) {
			if (s->name[i].handle == handle) {
				// fengnet_free(s->name[i].name);
				delete[] s->name[i].name;
				continue;
			} else if (i!=j) {
				s->name[j] = s->name[i];
			}
			++j;
		}
		s->name_count = j;
	} else {
		ctx = NULL;
	}

	lock.unlock();

	if (ctx) {
		// release ctx may call skynet_handle_* , so wunlock first.
		FengnetServer::serverInst->fengnet_context_release(ctx);
	}

	return ret;
}

fengnet_context* FengnetHandle::fengnet_handle_grab(uint32_t handle) {
	handle_storage *s = H;
	fengnet_context * result = nullptr;

	std::shared_lock<std::shared_mutex> lock(s->lock);

	uint32_t hash = handle & (s->slot_size-1);
	fengnet_context * ctx = s->slot[hash];
	if (ctx && FengnetServer::serverInst->fengnet_context_handle(ctx) == handle) {
		result = ctx;
		FengnetServer::serverInst->fengnet_context_grab(result);
	}

	// rwlock_runlock(&s->lock);

	return result;
}

uint32_t FengnetHandle::fengnet_handle_findname(const char* name) {
	handle_storage* s = H;

	std::shared_lock<std::shared_mutex> lock(s->lock);

	uint32_t handle = 0;

	int begin = 0;
	int end = s->name_count - 1;
	while (begin<=end) {
		int mid = (begin+end)/2;
		handle_name* n = &s->name[mid];
		int c = strcmp(n->name, name);
		if (c==0) {
			handle = n->handle;
			break;
		}
		if (c<0) {
			begin = mid + 1;
		} else {
			end = mid - 1;
		}
	}

	return handle;
}

void FengnetHandle::_insert_name_before(handle_storage* s, char* name, uint32_t handle, int before) {
	if (s->name_count >= s->name_cap) {	// 2倍动态扩容
		s->name_cap *= 2;
		assert(s->name_cap <= MAX_SLOT_SIZE);
		// handle_name* n = fengnet_malloc(s->name_cap * sizeof(handle_name));
		handle_name* n = new handle_name[s->name_cap]();
		int i;
		for (i=0;i<before;i++) {
			n[i] = s->name[i];
		}
		for (i=before;i<s->name_count;i++) {
			n[i+1] = s->name[i];
		}
		delete[] s->name;
		s->name = n;
	} else {
		int i;
		for (i=s->name_count;i>before;i--) {
			s->name[i] = s->name[i-1];
		}
	}
	s->name[before].name = name;
	s->name[before].handle = handle;
	s->name_count ++;
}

const char* FengnetHandle::_insert_name(handle_storage* s, const char* name, uint32_t handle) {
	int begin = 0;
	int end = s->name_count - 1;
	// 这里是一个二分查找，也就是说在插入时需要按name字母序插入
	while (begin<=end) {
		int mid = (begin+end)/2;
		handle_name* n = &s->name[mid];
		int c = strcmp(n->name, name);
		if (c==0) {
			return NULL;
		}
		if (c<0) {
			begin = mid + 1;
		} else {
			end = mid - 1;
		}
	}
	char * result = fengnet_strdup(name);

	_insert_name_before(s, result, handle, begin);

	return result;
}

const char* FengnetHandle::fengnet_handle_namehandle(uint32_t handle, const char* name){
	std::unique_lock<std::shared_mutex> lock(H->lock);

	const char * ret = _insert_name(H, name, handle);

	return ret;
}

void FengnetHandle::fengnet_handle_init(int harbor) {
	assert(H==NULL);
	// handle_storage * s = fengnet_malloc(sizeof(*H));
	handle_storage* s = new handle_storage();
	s->slot_size = DEFAULT_SLOT_SIZE;
	// s->slot = fengnet_malloc(s->slot_size * sizeof(fengnet_context *));
	s->slot = new fengnet_context*[s->slot_size]();
	memset(s->slot, 0, s->slot_size * sizeof(fengnet_context *));

	// rwlock_init(&s->lock);
	// reserve 0 for system
	s->harbor = (uint32_t) (harbor & 0xff) << HANDLE_REMOTE_SHIFT;
	s->handle_index = 1;
	s->name_cap = 2;
	s->name_count = 0;
	// s->name = fengnet_malloc(s->name_cap * sizeof(handle_name));
	s->name = new handle_name[s->name_cap]();

	H = s;
	// fengnetServer = make_shared<FengnetServer>();

	// Don't need to free H
}
