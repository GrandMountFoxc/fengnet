#include "fengnet.h"
#include "fengnet_handle.h"
#include "fengnet_mq.h"
#include "fengnet_server.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#define LOG_MESSAGE_SIZE 256

// shared_ptr<FengnetHandle> fengnetHandle = make_shared<FengnetHandle>();
// shared_ptr<FengnetServer> fengnetServer = make_shared<FengnetServer>();

void fengnet_error(fengnet_context* context, const char* msg, ...) {
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

