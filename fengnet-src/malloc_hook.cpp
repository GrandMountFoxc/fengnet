#include "malloc_hook.h"
// turn on MEMORY_CHECK can do more memory check, such as double free
// #define MEMORY_CHECK

#define MEMORY_ALLOCTAG 0x20140605
#define MEMORY_FREETAG 0x0badf00d

using namespace std;

static atomic<size_t> _used_memory(0);
static atomic<size_t> _memory_block(0);

struct mem_data {
	atomic<unsigned long> handle;
	atomic<size_t> allocated;
};

struct mem_cookie {
	uint32_t handle;
#ifdef MEMORY_CHECK
	uint32_t dogtag;
#endif
};

#define SLOT_SIZE 0x10000
#define PREFIX_SIZE sizeof(struct mem_cookie)

static struct mem_data mem_stats[SLOT_SIZE];


#ifndef NOUSE_JEMALLOC

// extern "C"
// {
//    #include "jemalloc/jemalloc.h"
// }

// for skynet_lalloc use
#define raw_realloc realloc
#define raw_free free

void memory_info_dump(const char* opts) {
	Fengnet::inst->fengnet_error(nullptr, "No jemalloc");
}

size_t mallctl_int64(const char* name, size_t* newval) {
	Fengnet::inst->fengnet_error(nullptr, "No jemalloc : mallctl_int64 %s.", name);
	return 0;
}

int mallctl_opt(const char* name, int* newval) {
	Fengnet::inst->fengnet_error(nullptr, "No jemalloc : mallctl_opt %s.", name);
	return 0;
}

bool mallctl_bool(const char* name, bool* newval) {
	Fengnet::inst->fengnet_error(nullptr, "No jemalloc : mallctl_bool %s.", name);
	return 0;
}

int mallctl_cmd(const char* name) {
	Fengnet::inst->fengnet_error(nullptr, "No jemalloc : mallctl_cmd %s.", name);
	return 0;
}

// 深拷贝？
char* fengnet_strdup(const char *str) {
	size_t sz = strlen(str);
	char* ret = static_cast<char*>(malloc(sz+1));
	memcpy(ret, str, sz+1);
	return ret;
}

string fengnet_strdup(string str){
	string ret(str);
	return ret;
}

void* fengnet_lalloc(void *ptr, size_t osize, size_t nsize) {
	if (nsize == 0) {
		raw_free(ptr);
		return NULL;
	} else {
		return raw_realloc(ptr, nsize);
	}
}

size_t malloc_used_memory(void) {
	return atomic_load(&_used_memory);
}

size_t malloc_memory_block(void) {
	return atomic_load(&_memory_block);
}

void dump_c_mem() {
	int i;
	size_t total = 0;
	Fengnet::inst->fengnet_error(nullptr, "dump all service mem:");
	for(i=0; i<SLOT_SIZE; i++) {
		struct mem_data* data = &mem_stats[i];
		if(data->handle != 0 && data->allocated != 0) {
			total += data->allocated;
			int handle = data->handle;
			int allocated = data->allocated >> 10;
			int temp = data->allocated % 1024;
			Fengnet::inst->fengnet_error(nullptr, ":%08x -> %zdkb %db", handle, allocated, temp);
		}
	}
	Fengnet::inst->fengnet_error(nullptr, "+total: %zdkb",total >> 10);
}

int dump_mem_lua(lua_State *L) {
	int i;
	lua_newtable(L);
	for(i=0; i<SLOT_SIZE; i++) {
		struct mem_data* data = &mem_stats[i];
		if(data->handle != 0 && data->allocated != 0) {
			lua_pushinteger(L, data->allocated);
			lua_rawseti(L, -2, (lua_Integer)data->handle);
		}
	}
	return 1;
}

size_t malloc_current_memory(void) {
	uint32_t handle = Fengnet::inst->fengnet_current_handle();
	int i;
	for(i=0; i<SLOT_SIZE; i++) {
		struct mem_data* data = &mem_stats[i];
		if(data->handle == (uint32_t)handle && data->allocated != 0) {
			return (size_t) data->allocated;
		}
	}
	return 0;
}

void fengnet_debug_memory(const char *info) {
	// for debug use
	uint32_t handle = Fengnet::inst->fengnet_current_handle();
	size_t mem = malloc_current_memory();
	fprintf(stderr, "[:%08x] %s %p\n", handle, info, (void *)mem);
}


// static atomic<size_t> *
// get_allocated_field(uint32_t handle) {
// 	int h = (int)(handle & (SLOT_SIZE - 1));
// 	struct mem_data *data = &mem_stats[h];
// 	uint32_t old_handle = data->handle;
// 	ssize_t old_alloc = (ssize_t)data->allocated;
// 	if(old_handle == 0 || old_alloc <= 0) {
// 		// data->allocated may less than zero, because it may not count at start.
// 		// if(!atomic_compare_exchange_weak(&data->handle, old_handle, handle)) {
// 		if(!data->handle.compare_exchange_weak(old_handle, handle)) {
// 			return 0;
// 		}
// 		if (old_alloc < 0) {
// 			// atomic_compare_exchange_weak(&data->allocated, (size_t)old_alloc, 0);
// 			size_t excepted_val = old_alloc;
// 			data->allocated.compare_exchange_weak(excepted_val, 0);
// 		}
// 	}
// 	if(data->handle != handle) {
// 		return 0;
// 	}
// 	return &data->allocated;
// }

// inline static void
// update_xmalloc_stat_alloc(uint32_t handle, size_t __n) {
// 	_used_memory += __n;
// 	_memory_block++;
// 	atomic<size_t>* allocated = get_allocated_field(handle);
// 	if(allocated) {
// 		allocated += __n;
// 	}
// }

// inline static void
// update_xmalloc_stat_free(uint32_t handle, size_t __n) {
// 	_used_memory -= __n;
// 	_memory_block--;
// 	atomic<size_t> * allocated = get_allocated_field(handle);
// 	if(allocated) {
// 		allocated -= __n;
// 	}
// }

// 在分配内存的末尾加上coocik
// 加上了线程handle和用来检测内存泄漏的dogtag
// inline static void*
// fill_prefix(char* ptr) {
// 	uint32_t handle = fengnet_current_handle();
// 	size_t size = je_malloc_usable_size(ptr);	// malloc_usable_size获取分配内存大小
// 	struct mem_cookie *p = (struct mem_cookie *)(ptr + size - sizeof(struct mem_cookie));
// 	memcpy(&p->handle, &handle, sizeof(handle));
// #ifdef MEMORY_CHECK
// 	uint32_t dogtag = MEMORY_ALLOCTAG;
// 	memcpy(&p->dogtag, &dogtag, sizeof(dogtag));
// #endif
// 	update_xmalloc_stat_alloc(handle, size);
// 	return ptr;
// }

// inline static void*
// clean_prefix(char* ptr) {
// 	size_t size = je_malloc_usable_size(ptr);
// 	struct mem_cookie *p = (struct mem_cookie *)(ptr + size - sizeof(struct mem_cookie));
// 	uint32_t handle;
// 	memcpy(&handle, &p->handle, sizeof(handle));
// #ifdef MEMORY_CHECK
// 	uint32_t dogtag;
// 	memcpy(&dogtag, &p->dogtag, sizeof(dogtag));
// 	if (dogtag == MEMORY_FREETAG) {
// 		fprintf(stderr, "xmalloc: double free in :%08x\n", handle);
// 	}
// 	assert(dogtag == MEMORY_ALLOCTAG);	// memory out of bounds
// 	dogtag = MEMORY_FREETAG;
// 	memcpy(&p->dogtag, &dogtag, sizeof(dogtag));
// #endif
// 	update_xmalloc_stat_free(handle, size);
// 	return ptr;
// }

// static void malloc_oom(size_t size) {
// 	fprintf(stderr, "xmalloc: Out of memory trying to allocate %zu bytes\n",
// 		size);
// 	fflush(stderr);
// 	abort();
// }

// void
// memory_info_dump(const char* opts) {
// 	je_malloc_stats_print(0,0, opts);
// }

// bool
// mallctl_bool(const char* name, bool* newval) {
// 	bool v = 0;
// 	size_t len = sizeof(v);
// 	if(newval) {
// 		je_mallctl(name, &v, &len, newval, sizeof(bool));
// 	} else {
// 		je_mallctl(name, &v, &len, NULL, 0);
// 	}
// 	return v;
// }

// int
// mallctl_cmd(const char* name) {
// 	return je_mallctl(name, NULL, NULL, NULL, 0);
// }

// size_t
// mallctl_int64(const char* name, size_t* newval) {
// 	size_t v = 0;
// 	size_t len = sizeof(v);
// 	if(newval) {
// 		je_mallctl(name, &v, &len, newval, sizeof(size_t));
// 	} else {
// 		je_mallctl(name, &v, &len, NULL, 0);
// 	}
// 	// skynet_error(NULL, "name: %s, value: %zd\n", name, v);
// 	return v;
// }

// int
// mallctl_opt(const char* name, int* newval) {
// 	int v = 0;
// 	size_t len = sizeof(v);
// 	if(newval) {
// 		int ret = je_mallctl(name, &v, &len, newval, sizeof(int));
// 		if(ret == 0) {
// 			fengnet_error(NULL, "set new value(%d) for (%s) succeed\n", *newval, name);
// 		} else {
// 			fengnet_error(NULL, "set new value(%d) for (%s) failed: error -> %d\n", *newval, name, ret);
// 		}
// 	} else {
// 		je_mallctl(name, &v, &len, NULL, 0);
// 	}

// 	return v;
// }

// hook : malloc, realloc, free, calloc

// void *
// fengnet_malloc(size_t size) {
// 	void* ptr = je_malloc(size + PREFIX_SIZE);
// 	if(!ptr) malloc_oom(size);
// 	return fill_prefix(ptr);
// }

// void *
// fengnet_realloc(void *ptr, size_t size) {
// 	if (ptr == NULL) return fengnet_malloc(size);

// 	void* rawptr = clean_prefix(ptr);
// 	void *newptr = je_realloc(rawptr, size+PREFIX_SIZE);
// 	if(!newptr) malloc_oom(size);
// 	return fill_prefix(newptr);
// }

// void
// fengnet_free(void *ptr) {
// 	if (ptr == NULL) return;
// 	void* rawptr = clean_prefix(ptr);
// 	je_free(rawptr);
// }

// void *
// fengnet_calloc(size_t nmemb,size_t size) {
// 	void* ptr = je_calloc(nmemb + ((PREFIX_SIZE+size-1)/size), size );
// 	if(!ptr) malloc_oom(size);
// 	return fill_prefix(ptr);
// }

// void *
// fengnet_memalign(size_t alignment, size_t size) {
// 	void* ptr = je_memalign(alignment, size + PREFIX_SIZE);
// 	if(!ptr) malloc_oom(size);
// 	return fill_prefix(ptr);
// }

// void *
// fengnet_aligned_alloc(size_t alignment, size_t size) {
// 	void* ptr = je_aligned_alloc(alignment, size + (size_t)((PREFIX_SIZE + alignment -1) & ~(alignment-1)));
// 	if(!ptr) malloc_oom(size);
// 	return fill_prefix(ptr);
// }

// int
// fengnet_posix_memalign(void **memptr, size_t alignment, size_t size) {
// 	int err = je_posix_memalign(memptr, alignment, size + PREFIX_SIZE);
// 	if (err) malloc_oom(size);
// 	fill_prefix(*memptr);
// 	return err;
// }

#else

// for skynet_lalloc use
#define raw_realloc realloc
#define raw_free free

void
memory_info_dump(const char* opts) {
	fengnet_error(NULL, "No jemalloc");
}

size_t
mallctl_int64(const char* name, size_t* newval) {
	fengnet_error(NULL, "No jemalloc : mallctl_int64 %s.", name);
	return 0;
}

int
mallctl_opt(const char* name, int* newval) {
	fengnet_error(NULL, "No jemalloc : mallctl_opt %s.", name);
	return 0;
}

bool
mallctl_bool(const char* name, bool* newval) {
	fengnet_error(NULL, "No jemalloc : mallctl_bool %s.", name);
	return 0;
}

int
mallctl_cmd(const char* name) {
	fengnet_error(NULL, "No jemalloc : mallctl_cmd %s.", name);
	return 0;
}

#endif

// size_t
// malloc_used_memory(void) {
// 	return atomic_load(&_used_memory);
// }

// size_t
// malloc_memory_block(void) {
// 	return atomic_load(&_memory_block);
// }

// void
// dump_c_mem() {
// 	int i;
// 	size_t total = 0;
// 	fengnet_error(NULL, "dump all service mem:");
// 	for(i=0; i<SLOT_SIZE; i++) {
// 		struct mem_data* data = &mem_stats[i];
// 		if(data->handle != 0 && data->allocated != 0) {
// 			total += data->allocated;
// 			fengnet_error(NULL, ":%08x -> %zdkb %db", data->handle, data->allocated >> 10, (int)(data->allocated % 1024));
// 		}
// 	}
// 	fengnet_error(NULL, "+total: %zdkb",total >> 10);
// }

// int
// dump_mem_lua(lua_State *L) {
// 	int i;
// 	lua_newtable(L);
// 	for(i=0; i<SLOT_SIZE; i++) {
// 		struct mem_data* data = &mem_stats[i];
// 		if(data->handle != 0 && data->allocated != 0) {
// 			lua_pushinteger(L, data->allocated);
// 			lua_rawseti(L, -2, (lua_Integer)data->handle);
// 		}
// 	}
// 	return 1;
// }

// size_t
// malloc_current_memory(void) {
// 	uint32_t handle = fengnet_current_handle();
// 	int i;
// 	for(i=0; i<SLOT_SIZE; i++) {
// 		struct mem_data* data = &mem_stats[i];
// 		if(data->handle == (uint32_t)handle && data->allocated != 0) {
// 			return (size_t) data->allocated;
// 		}
// 	}
// 	return 0;
// }

// void
// fengnet_debug_memory(const char *info) {
// 	// for debug use
// 	uint32_t handle = fengnet_current_handle();
// 	size_t mem = malloc_current_memory();
// 	fprintf(stderr, "[:%08x] %s %p\n", handle, info, (void *)mem);
// }
