#include "fengnet_server.h"
#include "fengnet_handle.h"
#include "fengnet_harbor.h"

FengnetHarbor* FengnetHarbor::harborInst;
FengnetHarbor::FengnetHarbor(){
    harborInst = this;
}

inline int FengnetHarbor::invalid_type(int type){
    return type != PTYPE_SYSTEM && type != PTYPE_HARBOR;
}

void FengnetHarbor::fengnet_harbor_init(int harbor){
    HARBOR = (unsigned int)harbor << HANDLE_REMOTE_SHIFT;
    // fengnetServer = make_shared<FengnetServer>();
}

// 调用skynet_context_send发送消息
void FengnetHarbor::fengnet_harbor_send(remote_message *rmsg, uint32_t source, int session) {
	assert(invalid_type(rmsg->type) && REMOTE);		
	// 这里之所以可以直接用REMOTE作为ctx传入，是因为在skynet_harbor_start函数里面就进行了赋值
	FengnetServer::serverInst->fengnet_context_send(REMOTE, rmsg, sizeof(*rmsg) , source, PTYPE_SYSTEM , session);
}

void FengnetHarbor::fengnet_harbor_start(fengnet_context* ctx){
    // the HARBOR must be reserved to ensure the pointer is valid.
	// It will be released at last by calling skynet_harbor_exit
	FengnetServer::serverInst->fengnet_context_reserve(ctx);
	REMOTE = ctx;
}

void FengnetHarbor::fengnet_harbor_exit(){
    fengnet_context* ctx = REMOTE;
    REMOTE = nullptr;
    if(ctx){
        FengnetServer::serverInst->fengnet_context_release(ctx);
    }
}

int FengnetHarbor::fengnet_harbor_message_isremote(uint32_t handle){
    assert(HARBOR != ~0);
    int h = (handle & ~HANDLE_MASK);
    return h!=HARBOR && h!=0;
}