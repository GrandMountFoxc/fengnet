#include "fengnet_module.h"

FengnetModule* FengnetModule::moduleInst;
FengnetModule::FengnetModule(){
	moduleInst = this;
}

void* FengnetModule::_try_open(modules *m, const char * name) {
	const char *l;	
	const char * path = m->path;
	size_t path_size = strlen(path);
	size_t name_size = strlen(name);
	char prefix[name_size+3] = "lib";
	strcat(prefix, name);

	int sz = path_size + name_size + 3;
	//search path
	void * dl = nullptr;
	char tmp[sz];
	do
	{
		memset(tmp,0,sz);
		while (*path == ';') path++;
		if (*path == '\0') break;
		// char *strchr(const char *str, int c) strchr() 
		// 用于查找字符串中的一个字符，并返回该字符在字符串中第一次出现的位置。
		l = strchr(path, ';');		
		if (l == nullptr) l = path + strlen(path);	// 如果没找到；就说明只有一个动态库路径了，直接让l指向\0
		int len = l - path;
		int i;
		for (i=0;path[i]!='?' && i < len ;i++) {
			tmp[i] = path[i];
		}
		memcpy(tmp+i,prefix,name_size+3);
		if (path[i] == '?') {
			strncpy(tmp+i+name_size+3,path+i+1,len - i - 1);
		} else {
			fprintf(stderr,"Invalid C service path\n");
			exit(1);
		}
		// void *dlopen(const char *filename, int flag);
		// 如果设置为 RTLD_NOW 的话，则立刻计算；如果设置的是 RTLD_LAZY，则在需要的时候才计算。
		// RTLD_GLOBAL：动态库中定义的符号可被其后打开的其它库重定位。
		dl = dlopen(tmp, RTLD_NOW | RTLD_GLOBAL);
		path = l;
	}while(dl == nullptr);

	if (dl == nullptr) {
		fprintf(stderr, "try open %s failed : %s\n",prefix,dlerror());
	}

	return dl;
}

fengnet_module* FengnetModule::_query(const char * name) {
	int i;
	for (i=0;i<M->count;i++) {
		if (strcmp(M->m[i].name,name)==0) {
			return &M->m[i];
		}
	}
	return nullptr;
}

void* FengnetModule::get_api(fengnet_module *mod, const char *api_name) {
	size_t name_size = strlen(mod->name);	// mod->name表示动态库的名字
	size_t api_size = strlen(api_name);
	char tmp[name_size + api_size + 1];

	// 把mod->name和api_name结合起来，
	// 比如mod->name=logger,api_name=_create,经过处理得到logger_create
	// 相当于动态库函数名按照这个规则设计的，这里就能找到相应的函数并加载
	memcpy(tmp, mod->name, name_size);
	memcpy(tmp+name_size, api_name, api_size+1);

	// char *strrchr(const char *str, int c)
	// 在参数 str 所指向的字符串中搜索最后一次出现字符 c（一个无符号字符）的位置。
	// 和strchr相反，strchr找第一个，strrchr找最后一个
	char *ptr = strrchr(tmp, '.');
	if (ptr == nullptr) {
		ptr = tmp;
	} else {
		ptr = ptr + 1;
	}
	// void *dlsym(void *handle, const char *symbol);
	// 在打开的动态库中查找符号的值。
	return dlsym(mod->module, ptr);
}

int FengnetModule::open_sym(fengnet_module *mod) {
	mod->create = reinterpret_cast<fengnet_dl_create>(get_api(mod, "_create"));
	mod->init = reinterpret_cast<fengnet_dl_init>(get_api(mod, "_init"));
	mod->release = reinterpret_cast<fengnet_dl_release>(get_api(mod, "_release"));
	mod->signal = reinterpret_cast<fengnet_dl_signal>(get_api(mod, "_signal"));	// logger库里面没有logger_signal，这种情况下返回值为NULL

	return mod->init == nullptr;	// 这里就相当于判断了这个动态有没有初始化函数，如果连初始化函数都没有就不用加载了
}

fengnet_module* FengnetModule::fengnet_module_query(const char * name) {
	fengnet_module* result = _query(name);
	if (result)
		return result;

	lock_guard<SpinLock> lock(M->lock);

	result = _query(name); // double check 单例模式的双检锁？

	if (result == nullptr && M->count < MAX_MODULE_TYPE) {
		int index = M->count;
		void* dl = _try_open(M,name);	// 打开动态库文件
		if (dl) {
			M->m[index].name = name;
			M->m[index].module = dl;

			if (open_sym(&M->m[index]) == 0) {
				//skynet_strdup函数里面就是做了一次memcpy，但这有什么意义，深拷贝？
				M->m[index].name = fengnet_strdup(name);	// malloc_hook.c 
				M->count ++;
				result = &M->m[index];
			}
		}
	}

	return result;
}

// 如果有_create函数就返回函数地址
void* FengnetModule::fengnet_module_instance_create(fengnet_module* m) {
	if (m->create) {
		return m->create();
	} else {
		return (void*)(intptr_t)(~0);
	}
}

int FengnetModule::fengnet_module_instance_init(fengnet_module* m, void* inst, fengnet_context* ctx, const char* parm){
    return m->init(inst, ctx, parm);
}

void FengnetModule::fengnet_module_instance_release(fengnet_module* m, void* inst){
    if(m->release){
        m->release(inst);
    }
}

void FengnetModule::fengnet_module_instance_signal(fengnet_module* m, void* inst, int signal) {
	if (m->signal) {
		m->signal(inst, signal);
	}
}

void FengnetModule::fengnet_module_init(const char* path){
    modules* m = new modules();
    m->count = 0;
    m->path = fengnet_strdup(path);     // skynet_strdup函数里面就是做了一次memcpy，但这有什么意义，深拷贝？

	M = m;
}