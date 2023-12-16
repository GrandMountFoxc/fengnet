#ifndef FENGNET_HARBOR_H
#define FENGNET_HARBOR_H

#include "fengnet_malloc.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <memory>

using namespace std;

// class FengnetServer;
struct fengnet_context;

#define GLOBALNAME_LENGTH 16
#define REMOTE_MAX 256

struct remote_name{
    char name[GLOBALNAME_LENGTH];
    uint32_t handle;
};

struct remote_message{
    remote_name destination;
    const void* message;
    size_t sz;
    int type;
};

class FengnetHarbor{
public:
    static FengnetHarbor* harborInst;
public:
    FengnetHarbor();
    FengnetHarbor(FengnetHarbor&) = delete;
    FengnetHarbor& operator=(FengnetHarbor&) = delete;
    void fengnet_harbor_init(int harbor);
    void fengnet_harbor_start(fengnet_context* ctx);
    void fengnet_harbor_send(remote_message* rmsg, uint32_t source, int session);
    int fengnet_harbor_message_isremote(uint32_t handle);
    void fengnet_harbor_exit();
private:
    fengnet_context* REMOTE = 0;
    unsigned int HARBOR = ~0;
    // shared_ptr<FengnetServer> fengnetServer;
private:
    inline int invalid_type(int type);
};

#endif