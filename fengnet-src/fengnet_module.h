#ifndef FENGNET_MODULE_H
#define FENGNET_MODULE_H

#define MAX_MODULE_TYPE 32

#include "fengnet_server.h"
#include "malloc_hook.h"
#include "fengnet_malloc.h"
#include "spinlock.h"

#include <thread>
#include <mutex>
#include <cstring>
#include <dlfcn.h>

typedef void * (*fengnet_dl_create)(void);
typedef int (*fengnet_dl_init)(void * inst, fengnet_context *, const char * parm);
typedef void (*fengnet_dl_release)(void * inst);
typedef void (*fengnet_dl_signal)(void * inst, int signal);

struct fengnet_module{
    const char* name;
    void* module;
    fengnet_dl_create create;
    fengnet_dl_init init;
    fengnet_dl_release release;
    fengnet_dl_signal signal;
};

struct modules {
	int count;
	SpinLock lock;      // 自旋锁是否有问题
	const char * path;
	fengnet_module m[MAX_MODULE_TYPE];
};

class FengnetModule{
public:
    static FengnetModule* moduleInst;
public:
    FengnetModule();
    FengnetModule(const FengnetModule&) = delete;
    FengnetModule& operator=(const FengnetModule&) = delete;
    fengnet_module* fengnet_module_query(const char* name);
    void* fengnet_module_instance_create(fengnet_module*);
    int fengnet_module_instance_init(fengnet_module*, void* inst, fengnet_context* ctx, const char* parm);
    void fengnet_module_instance_release(fengnet_module*, void* inst);
    void fengnet_module_instance_signal(fengnet_module*, void* inst, int signal);

    void fengnet_module_init(const char* path);
private:
    modules* M;
private:
    int open_sym(fengnet_module *mod);
    void* get_api(fengnet_module *mod, const char *api_name);
    fengnet_module* _query(const char * name);
    void* _try_open(modules *m, const char * name);
};

#endif