#include "fengnet.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <ctime>

struct logger {
	FILE* handle;
	char* filename;
	uint32_t starttime;
	int close;
};

extern "C" logger* logger_create() {
	logger* inst = new logger();
	inst->handle = nullptr;
	inst->close = 0;
	inst->filename = nullptr;

	return inst;
}

extern "C" void logger_release(logger* inst) {
	if (inst->close) {
		fclose(inst->handle);
	}
	delete[] inst->filename;
	delete inst;
}

#define SIZETIMEFMT	250

static int timestring(logger* inst, char tmp[SIZETIMEFMT]) {
	uint64_t now = Fengnet::inst->fengnet_now();
	time_t ti = now/100 + inst->starttime;
	tm info;
	(void)localtime_r(&ti,&info);
	strftime(tmp, SIZETIMEFMT, "%d/%m/%y %H:%M:%S", &info);
	return now % 100;
}

static int logger_cb(fengnet_context* context, void* ud, int type, int session, uint32_t source, const void * msg, size_t sz) {
	logger* inst = (logger*)ud;
	switch (type) {
	case PTYPE_SYSTEM:
		if (inst->filename) {
			inst->handle = freopen(inst->filename, "a", inst->handle);
		}
		break;
	case PTYPE_TEXT:
		if (inst->filename) {
			char tmp[SIZETIMEFMT];
			int csec = timestring((logger*)ud, tmp);
			fprintf(inst->handle, "%s.%02d ", tmp, csec);
		}
		fprintf(inst->handle, "[:%08x] ", source);
		fwrite(msg, sz , 1, inst->handle);
		fprintf(inst->handle, "\n");
		fflush(inst->handle);
		break;
	}

	return 0;
}

extern "C" int logger_init(logger* inst, fengnet_context* ctx, const char* parm) {
	const char* r = Fengnet::inst->fengnet_command(ctx, "STARTTIME", NULL);
	inst->starttime = strtoul(r, NULL, 10);
	if (parm) {
		inst->handle = fopen(parm,"a");
		if (inst->handle == NULL) {
			return 1;
		}
		inst->filename = new char[strlen(parm)+1];
		strcpy(inst->filename, parm);
		inst->close = 1;
	} else {
		inst->handle = stdout;
	}
	if (inst->handle) {
		Fengnet::inst->fengnet_callback(ctx, inst, logger_cb);
		return 0;
	}
	return 1;
}
