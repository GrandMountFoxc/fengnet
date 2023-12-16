#include "fengnet.h"
#include "fengnet_server.h"
#include "fengnet_handle.h"
#include "fengnet_timer.h"
#include "fengnet_mq.h"
#include "fengnet_module.h"
#include "fengnet_log.h"

Fengnet* Fengnet::inst;

const command_func Fengnet::cmd_funcs[CMD_MAX_NUM] = {
	{"TIMEOUT", cmd_timeout} ,
	{"REG", cmd_reg} ,
	{"QUERY", cmd_query} ,
	{"NAME", cmd_name} ,
	{"EXIT", cmd_exit} ,
	{"KILL", cmd_kill} ,
	{"LAUNCH", cmd_launch} ,
	{"GETENV", cmd_getenv} ,
	{"SETENV", cmd_setenv} ,
	{"STARTTIME", cmd_starttime} ,
	{"ABORT", cmd_abort} ,
	{"MONITOR", cmd_monitor} ,
	{"STAT", cmd_stat} ,
	{"LOGON", cmd_logon} ,
	{"LOGOFF", cmd_logoff} ,
	{"SIGNAL", cmd_signal} ,
	{nullptr, nullptr} ,
};

Fengnet::Fengnet(){
    inst = this;
}

void Fengnet::Start(){
    // fengnetServer = make_shared<FengnetServer>();
    // fengnetTimer = make_shared<FengnetTimer>();
    // fengnetHandle = make_shared<FengnetHandle>();
    // fengnetMQ = make_shared<FengnetMQ>();
	// fengnetModule = make_shared<FengnetModule>();
	// fengnetHarbor = make_shared<FengnetHarbor>();
}

void Fengnet::handle_exit(fengnet_context* context, uint32_t handle) {
	if (handle == 0) {
		handle = context->handle;
		fengnet_error(context, "KILL self");
	} else {
		fengnet_error(context, "KILL :%0x", handle);
	}
	if (G_NODE.monitor_exit) {
		fengnet_send(context,  handle, G_NODE.monitor_exit, PTYPE_CLIENT, 0, NULL, 0);
	}
	FengnetHandle::handleInst->fengnet_handle_retire(handle);
}

uint32_t Fengnet::tohandle(fengnet_context* context, const char* param) {
	uint32_t handle = 0;
	if (param[0] == ':') {
		handle = strtoul(param+1, NULL, 16);
	} else if (param[0] == '.') {
		handle = FengnetHandle::handleInst->fengnet_handle_findname(param+1);
	} else {
		fengnet_error(context, "Can't convert %s to handle",param);
	}

	return handle;
}

void Fengnet::id_to_hex(char* str, uint32_t id) {
	int i;
	static char hex[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
	str[0] = ':';
	for (i=0;i<8;i++) {
		str[i+1] = hex[(id >> ((7-i) * 4))&0xf];
	}
	str[9] = '\0';
}

const char* Fengnet::cmd_timeout(fengnet_context* context, const char* param) {
	char * session_ptr = nullptr;
	int ti = strtol(param, &session_ptr, 10);
	int session = FengnetServer::serverInst->fengnet_context_newsession(context);
	FengnetTimer::timerInst->fengnet_timeout(context->handle, ti, session);
	sprintf(context->result, "%d", session);
	return context->result;
}

const char* Fengnet::cmd_reg(fengnet_context* context, const char* param) {
	if (param == NULL || param[0] == '\0') {
		// int sprintf(char *str, const char *format, ...)
		// 发送格式化输出到 str 所指向的字符串
		// %x 无符号十六进制整数
		// 将回调函数的执行结果和 handle 拼接在一起，并返回
		sprintf(context->result, ":%x", context->handle);
		// context->result 是用来存放 context->cb 的执行结果的
		return context->result;
	} else if (param[0] == '.') {
		return FengnetHandle::handleInst->fengnet_handle_namehandle(context->handle, param + 1);
	} else {
		inst->fengnet_error(context, "Can't register global name %s in C", param);
		return nullptr;
	}
}

const char* Fengnet::cmd_query(fengnet_context* context, const char* param) {
	if (param[0] == '.') {
		uint32_t handle = FengnetHandle::handleInst->fengnet_handle_findname(param+1);
		if (handle) {
			sprintf(context->result, ":%x", handle);
			return context->result;
		}
	}
	return nullptr;
}

const char* Fengnet::cmd_name(fengnet_context* context, const char* param) {
	int size = strlen(param);
	char name[size+1];
	char handle[size+1];
	sscanf(param,"%s %s",name,handle);
	if (handle[0] != ':') {
		return nullptr;
	}
	uint32_t handle_id = strtoul(handle+1, nullptr, 16);
	if (handle_id == 0) {
		return nullptr;
	}
	if (name[0] == '.') {
		return FengnetHandle::handleInst->fengnet_handle_namehandle(handle_id, name + 1);
	} else {
		inst->fengnet_error(context, "Can't set global name %s in C", name);
	}
	return nullptr;
}

const char* Fengnet::cmd_exit(fengnet_context* context, const char* param) {
	inst->handle_exit(context, 0);
	return nullptr;
}

const char* Fengnet::cmd_kill(fengnet_context* context, const char* param) {
	uint32_t handle = inst->tohandle(context, param);
	if (handle) {
		inst->handle_exit(context, handle);
	}
	return nullptr;
}

const char* Fengnet::cmd_launch(fengnet_context* context, const char* param) {
	size_t sz = strlen(param);
	char tmp[sz+1];
	strcpy(tmp,param);
	char* args = tmp;
	char* mod = strsep(&args, " \t\r\n");
	args = strsep(&args, "\r\n");
	fengnet_context* instance = FengnetServer::serverInst->fengnet_context_new(mod,args);
	if (instance == nullptr) {
		return nullptr;
	} else {
		inst->id_to_hex(context->result, instance->handle);
		return context->result;
	}
}

const char* Fengnet::cmd_getenv(fengnet_context* context, const char* param) {
	return FengnetEnv::inst->fengnet_getenv(param);
}

const char* Fengnet::cmd_setenv(fengnet_context* context, const char* param) {
	size_t sz = strlen(param);
	char key[sz+1];
	int i;
	for (i=0;param[i] != ' ' && param[i];i++) {
		key[i] = param[i];
	}
	if (param[i] == '\0')
		return nullptr;

	key[i] = '\0';
	param += i+1;
	
	FengnetEnv::inst->fengnet_setenv(key,param);
	return nullptr;
}

const char* Fengnet::cmd_starttime(fengnet_context* context, const char* param) {
	uint32_t sec = FengnetTimer::timerInst->fengnet_starttime();
	sprintf(context->result,"%u",sec);
	return context->result;
}

const char* Fengnet::cmd_abort(fengnet_context* context, const char* param) {
	FengnetHandle::handleInst->fengnet_handle_retireall();
	return nullptr;
}

// 这里的G_NODE没法同步
const char* Fengnet::cmd_monitor(fengnet_context* context, const char* param) {
	uint32_t handle=0;
	if (param == nullptr || param[0] == '\0') {
		if (inst->G_NODE.monitor_exit) {
			// return current monitor serivce
			sprintf(context->result, ":%x", inst->G_NODE.monitor_exit);
			return context->result;
		}
		return nullptr;
	} else {
		handle = inst->tohandle(context, param);
	}
	inst->G_NODE.monitor_exit = handle;
	return nullptr;
}

const char* Fengnet::cmd_stat(fengnet_context* context, const char* param) {
	if (strcmp(param, "mqlen") == 0) {
		int len = FengnetMQ::mqInst->fengnet_mq_length(context->queue);
		sprintf(context->result, "%d", len);
	} else if (strcmp(param, "endless") == 0) {
		if (context->endless) {
			strcpy(context->result, "1");
			context->endless = false;
		} else {
			strcpy(context->result, "0");
		}
	} else if (strcmp(param, "cpu") == 0) {
		double t = (double)context->cpu_cost / 1000000.0;	// microsec
		sprintf(context->result, "%lf", t);
	} else if (strcmp(param, "time") == 0) {
		if (context->profile) {
			uint64_t ti = FengnetTimer::timerInst->fengnet_thread_time() - context->cpu_start;
			double t = (double)ti / 1000000.0;	// microsec
			sprintf(context->result, "%lf", t);
		} else {
			strcpy(context->result, "0");
		}
	} else if (strcmp(param, "message") == 0) {
		sprintf(context->result, "%d", context->message_count);
	} else {
		context->result[0] = '\0';
	}
	return context->result;
}

const char* Fengnet::cmd_logon(fengnet_context* context, const char* param) {
	uint32_t handle = inst->tohandle(context, param);
	if (handle == 0)
		return nullptr;
	fengnet_context* ctx = FengnetHandle::handleInst->fengnet_handle_grab(handle);
	if (ctx == nullptr)
		return nullptr;
	FILE* f = nullptr;
	FILE* lastf = (FILE *)atomic_load(&ctx->logfile);
	if (lastf == nullptr) {
		f = FengnetLog::fengnet_log_open(context, handle);
		if (f) {
			// if (!ATOM_CAS_POINTER(&ctx->logfile, 0, (uintptr_t)f)) 
			uintptr_t excepted_val = 0;
			uintptr_t change_val = (uintptr_t)f;
			if(!ctx->logfile.compare_exchange_weak(excepted_val, change_val)) {
				// logfile opens in other thread, close this one.
				fclose(f);
			}
		}
	}
	FengnetServer::serverInst->fengnet_context_release(ctx);
	return nullptr;
}

const char* Fengnet::cmd_logoff(fengnet_context* context, const char* param) {
	uint32_t handle = inst->tohandle(context, param);
	if (handle == 0)
		return nullptr;
	fengnet_context* ctx = FengnetHandle::handleInst->fengnet_handle_grab(handle);
	if (ctx == nullptr)
		return nullptr;
	FILE * f = (FILE*)atomic_load(&ctx->logfile);
	if (f) {
		// logfile may close in other thread
		// if (ATOM_CAS_POINTER(&ctx->logfile, (uintptr_t)f, (uintptr_t)nullptr)) 
		uintptr_t excepted_val = uintptr_t(f);
		uintptr_t change_val = (uintptr_t)nullptr;
		if(!ctx->logfile.compare_exchange_weak(excepted_val, change_val)) {
			FengnetLog::fengnet_log_close(context, f, handle);
		}
	}
	FengnetServer::serverInst->fengnet_context_release(ctx);
	return nullptr;
}

const char* Fengnet::cmd_signal(fengnet_context* context, const char* param) {
	uint32_t handle = inst->tohandle(context, param);
	if (handle == 0)
		return nullptr;
	fengnet_context* ctx = FengnetHandle::handleInst->fengnet_handle_grab(handle);
	if (ctx == nullptr)
		return nullptr;
	param = strchr(param, ' ');
	int sig = 0;
	if (param) {
		sig = strtol(param, nullptr, 0);
	}
	// NOTICE: the signal function should be thread safe.
	FengnetModule::moduleInst->fengnet_module_instance_signal(ctx->mod, ctx->instance, sig);

	FengnetServer::serverInst->fengnet_context_release(ctx);
	return nullptr;
}

// 查找相应的命令，并返回命令函数的执行结果
// snlua 对 skynet_command 的调用形式为 skynet_command(ctx, "REG", NULL)
const char* Fengnet::fengnet_command(fengnet_context* context, const char* cmd , const char* param){
    const command_func* method = &cmd_funcs[0];
	while(method->name) {
		if (strcmp(cmd, method->name) == 0) {
			return method->func(context, param);
		}
		++method;
	}

	return nullptr;
}
void Fengnet::fengnet_error(fengnet_context* context, const char* msg, ...){    // fengnet_error.cpp
    static uint32_t logger = 0;
	if (logger == 0) {
		logger = FengnetHandle::handleInst->fengnet_handle_findname("logger");
	}
	if (logger == 0) {
		return;
	}

	char tmp[LOG_MESSAGE_SIZE];
	char* data = NULL;

	// VA_LIST 是在C语言中解决变参问题的一组宏，变参问题是指参数的个数不定，可以是传入一个参数也可以是多个;
	// 可变参数中的每个参数的类型可以不同,也可以相同;可变参数的每个参数并没有实际的名称与之相对应，用起来是很灵活。
	va_list ap;

	va_start(ap,msg);
	int len = vsnprintf(tmp, LOG_MESSAGE_SIZE, msg, ap);
	va_end(ap);
	if (len >=0 && len < LOG_MESSAGE_SIZE) {
		data = fengnet_strdup(tmp);
	} else {
		int max_size = LOG_MESSAGE_SIZE;
		for (;;) {
			max_size *= 2;
			// data = (char*)fengnet_malloc(max_size);
			data = new char[max_size];
			va_start(ap,msg);
			len = vsnprintf(data, max_size, msg, ap);
			va_end(ap);
			if (len < max_size) {
				break;
			}
			// fengnet_free(data);
			delete[] data;
		}
	}
	if (len < 0) {
		// fengnet_free(data);
		delete[] data;
		perror("vsnprintf error :");
		return;
	}


	fengnet_message smsg;
	if (context == NULL) {
		smsg.source = 0;
	} else {
		smsg.source = FengnetServer::serverInst->fengnet_context_handle(context);
	}
	smsg.session = 0;
	smsg.data = data;
	smsg.sz = len | ((size_t)PTYPE_TEXT << MESSAGE_TYPE_SHIFT);
	FengnetServer::serverInst->fengnet_context_push(logger, &smsg);
}

void Fengnet::_filter_args(fengnet_context * context, int type, int *session, void ** data, size_t * sz) {
	int needcopy = !(type & PTYPE_TAG_DONTCOPY);
	int allocsession = type & PTYPE_TAG_ALLOCSESSION;
	type &= 0xff;

	if (allocsession) {
		assert(*session == 0);
		*session = FengnetServer::serverInst->fengnet_context_newsession(context);
	}

	if (needcopy && *data) {
		char * msg = new char[*sz+1];
		memcpy(msg, *data, *sz);
		msg[*sz] = '\0';
		*data = msg;
	}

	*sz |= (size_t)type << MESSAGE_TYPE_SHIFT;
}

// skynet_send 使用了 source 和 destination 来标记消息的发送端和接收端，这两个参数的本质就是能够在全网范围内唯一标识一个服务的 handle
// handle 为一个 32 位无符号整数，其中高 8 位为 harbor id，用来表示服务所属的 skynet 节点，而剩余的 24 位则用于表示该 skynet 内的唯一一个服务
int Fengnet::fengnet_send(fengnet_context* context, uint32_t source, uint32_t destination , int type, int session, void* data, size_t sz) {
	if ((sz & MESSAGE_TYPE_MASK) != sz) {
		fengnet_error(context, "The message to %x is too large", destination);
		if (type & PTYPE_TAG_DONTCOPY) {
			delete data;
		}
		return -2;
	}
	//_filter_args:根据 type 中的 PTYPE_TAG_DONTCOPY 和 PTYPE_TAG_ALLOCSESSION 位域对参数进行一些相应的处理
    // PTYPE_TAG_DONTCOPY：表示不要拷贝 data 的副本，直接在 data 上进行处理
    // PTYPE_TAG_ALLOCSESSION: 表示为消息分配一个新的 session
	_filter_args(context, type, &session, (void **)&data, &sz);

	if (source == 0) {
		source = context->handle;
	}

	if (destination == 0) {
		if (data) {
			fengnet_error(context, "Destination address can't be 0");
			delete data;
			return -1;
		}

		return session;
	}
	// 不管最终调用的函数是 skynet_harbor_send 还是 skynet_context_push，最后都会回归到 skynet_mq_push 这个函数中
	// 因此，skynet 中发送消息的本质就是往目标服务的次级消息队列中压入消息
	if (FengnetHarbor::harborInst->fengnet_harbor_message_isremote(destination)) {
		remote_message * rmsg = new remote_message();
		rmsg->destination.handle = destination;
		rmsg->message = data;
		rmsg->sz = sz & MESSAGE_TYPE_MASK;
		rmsg->type = sz >> MESSAGE_TYPE_SHIFT;
		FengnetHarbor::harborInst->fengnet_harbor_send(rmsg, source, session);
	} else {
		fengnet_message smsg;
		smsg.source = source;
		smsg.session = session;
		smsg.data = data;
		smsg.sz = sz;

		if (FengnetServer::serverInst->fengnet_context_push(destination, &smsg)) {
			delete data;
			return -1;
		}
	}
	return session;
}

void Fengnet::copy_name(char name[GLOBALNAME_LENGTH], const char * addr) {
	int i;
	for (i=0;i<GLOBALNAME_LENGTH && addr[i];i++) {
		name[i] = addr[i];
	}
	for (;i<GLOBALNAME_LENGTH;i++) {
		name[i] = '\0';
	}
}

int Fengnet::fengnet_sendname(fengnet_context* context, uint32_t source, const char* addr , int type, int session, void* data, size_t sz) {
	if (source == 0) {
		source = context->handle;
	}
	uint32_t des = 0;
	if (addr[0] == ':') {
		des = strtoul(addr+1, NULL, 16);
	} else if (addr[0] == '.') {
		des = FengnetHandle::handleInst->fengnet_handle_findname(addr + 1);
		if (des == 0) {
			if (type & PTYPE_TAG_DONTCOPY) {
				delete data;
			}
			return -1;
		}
	} else {
		if ((sz & MESSAGE_TYPE_MASK) != sz) {
			fengnet_error(context, "The message to %s is too large", addr);
			if (type & PTYPE_TAG_DONTCOPY) {
				delete data;
			}
			return -2;
		}
		_filter_args(context, type, &session, (void **)&data, &sz);

		remote_message* rmsg = new remote_message();
		copy_name(rmsg->destination.name, addr);
		rmsg->destination.handle = 0;
		rmsg->message = data;
		rmsg->sz = sz & MESSAGE_TYPE_MASK;
		rmsg->type = sz >> MESSAGE_TYPE_SHIFT;

		FengnetHarbor::harborInst->fengnet_harbor_send(rmsg, source, session);
		return session;
	}

	return fengnet_send(context, source, des, type, session, data, sz);
}

// 将 cb 设置为 context 服务的回调函数，参数为 ud
// context : skynet_context类型，传入动态库context
// ud : void*类型，回调函数参数
// cb : skynet_cb类型，回调函数	
// 
// cb 这个变量的类型定义看不懂 
// typedef int (*skynet_cb)(struct skynet_context * context, void *ud, int type, int session, uint32_t source , const void * msg, size_t sz);
void Fengnet::fengnet_callback(fengnet_context* context, void* ud, fengnet_cb cb){
	context->cb = cb;
	context->cb_ud = ud;
}

uint64_t Fengnet::fengnet_now(){     // fengnet_timer.cpp
    return FengnetTimer::timerInst->fengnet_now();
}
