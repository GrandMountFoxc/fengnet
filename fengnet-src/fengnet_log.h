#ifndef FENGNET_LOG_H
#define FENGNET_LOG_H

#include "fengnet_env.h"
#include "fengnet.h"
#include "fengnet_timer.h"
#include "fengnet_socket.h"

#include <cstring>
#include <ctime>
#include <cstdio>
#include <cstdint>
#include <memory>

class FengnetLog{
public:
    static FILE* fengnet_log_open(fengnet_context* ctx, uint32_t handle);
    static void fengnet_log_close(fengnet_context* ctx, FILE* f, uint32_t handle);
    static void fengnet_log_output(FILE* f, uint32_t source, int type, int session, void* buffer, size_t sz);
private:
    // static shared_ptr<FengnetTimer> fengnetTimer;
private:
    static void log_blob(FILE* f, void* buffer, size_t sz);
    static void log_socket(FILE* f, fengnet_socket_message* message, size_t sz);
};

#endif