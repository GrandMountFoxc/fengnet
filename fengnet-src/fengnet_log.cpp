#include "fengnet_log.h"

// FengnetTimer FengnetLog::fengnetTimer = make_shared<FengnetTimer>();

FILE* FengnetLog::fengnet_log_open(fengnet_context* ctx, uint32_t handle) {
	const char* logpath = FengnetEnv::inst->fengnet_getenv("logpath");
	if (logpath == nullptr)
		return nullptr;
	size_t sz = strlen(logpath);
	char tmp[sz + 16];
	sprintf(tmp, "%s/%08x.log", logpath, handle);
	FILE* f = fopen(tmp, "ab");
	if (f) {
		uint32_t starttime = FengnetTimer::timerInst->fengnet_starttime();
		uint64_t currenttime = Fengnet::inst->fengnet_now();
		time_t ti = starttime + currenttime/100;
		Fengnet::inst->fengnet_error(ctx, "Open log file %s", tmp);
		fprintf(f, "open time: %u %s", (uint32_t)currenttime, ctime(&ti));
		fflush(f);
	} else {
		Fengnet::inst->fengnet_error(ctx, "Open log file %s fail", tmp);
	}
	return f;
}

void FengnetLog::fengnet_log_close(fengnet_context* ctx, FILE* f, uint32_t handle) {
	Fengnet::inst->fengnet_error(ctx, "Close log file :%08x", handle);
	fprintf(f, "close time: %u\n", (uint32_t)Fengnet::inst->fengnet_now());
	fclose(f);
}

void FengnetLog::log_blob(FILE* f, void* buffer, size_t sz) {
	size_t i;
	uint8_t* buf = static_cast<uint8_t*>(buffer);
	for (i=0;i!=sz;i++) {
		fprintf(f, "%02x", buf[i]);
	}
}

void FengnetLog::log_socket(FILE* f, fengnet_socket_message* message, size_t sz) {
	fprintf(f, "[socket] %d %d %d ", message->type, message->id, message->ud);

	if (message->buffer == NULL) {
		const char* buffer = reinterpret_cast<const char*>(message + 1);
		sz -= sizeof(*message);
		const char* eol = static_cast<const char*>(memchr(buffer, '\0', sz));
		if (eol) {
			sz = eol - buffer;
		}
		fprintf(f, "[%*s]", (int)sz, static_cast<const char*>(buffer));
	} else {
		sz = message->ud;
		log_blob(f, message->buffer, sz);
	}
	fprintf(f, "\n");
	fflush(f);
}

void FengnetLog::fengnet_log_output(FILE* f, uint32_t source, int type, int session, void* buffer, size_t sz) {
	if (type == PTYPE_SOCKET) {
		log_socket(f, static_cast<fengnet_socket_message*>(buffer), sz);
	} else {
		uint32_t ti = (uint32_t)Fengnet::inst->fengnet_now();
		fprintf(f, ":%08x %d %d %u ", source, type, session, ti);
		log_blob(f, buffer, sz);
		fprintf(f,"\n");
		fflush(f);
	}
}
