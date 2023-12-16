#include "fengnet_socket.h"

FengnetSocket* FengnetSocket::socketInst;
FengnetSocket::FengnetSocket(){
	socketInst = this;
}

void FengnetSocket::fengnet_socket_init(){
	// fengnetServer = make_shared<FengnetServer>();
    SOCKET_SERVER = socket_server_create(FengnetTimer::timerInst->fengnet_now());
}

void FengnetSocket::fengnet_socket_exit() {
	socket_server_exit(SOCKET_SERVER);
}

void FengnetSocket::forward_message(int type, bool padding, struct socket_message * result) {
	struct fengnet_socket_message *sm;
	size_t sz = sizeof(*sm);
	if (padding) {
		if (result->data) {
			size_t msg_sz = strlen(result->data);
			if (msg_sz > 128) {
				msg_sz = 128;
			}
			sz += msg_sz;
		} else {
			result->data = "";
		}
	}
	sm = (struct fengnet_socket_message *)malloc(sz);
	sm->type = type;
	sm->id = result->id;
	sm->ud = result->ud;
	if (padding) {
		sm->buffer = NULL;
		memcpy(sm+1, result->data, sz - sizeof(*sm));
	} else {
		sm->buffer = result->data;
	}

	struct fengnet_message message;
	message.source = 0;
	message.session = 0;
	message.data = sm;
	message.sz = sz | ((size_t)PTYPE_SOCKET << MESSAGE_TYPE_SHIFT);
	
	if (FengnetServer::serverInst->fengnet_context_push((uint32_t)result->opaque, &message)) {
		// todo: report somewhere to close socket
		// don't call skynet_socket_close here (It will block mainloop)
		free(sm->buffer);
		free(sm);
	}
}

int FengnetSocket::fengnet_socket_poll() {
	socket_server *ss = SOCKET_SERVER;
	assert(ss);
	socket_message result;
	int more = 1;
	int type = socket_server_poll(ss, &result, &more);
	switch (type) {
	case SOCKET_EXIT:
		return 0;
	case SOCKET_DATA:
		forward_message(FENGNET_SOCKET_TYPE_DATA, false, &result);
		break;
	case SOCKET_CLOSE:
		forward_message(FENGNET_SOCKET_TYPE_CLOSE, false, &result);
		break;
	case SOCKET_OPEN:
		forward_message(FENGNET_SOCKET_TYPE_CONNECT, true, &result);
		break;
	case SOCKET_ERR:
		forward_message(FENGNET_SOCKET_TYPE_ERROR, true, &result);
		break;
	case SOCKET_ACCEPT:
		forward_message(FENGNET_SOCKET_TYPE_ACCEPT, true, &result);
		break;
	case SOCKET_UDP:
		forward_message(FENGNET_SOCKET_TYPE_UDP, false, &result);
		break;
	case SOCKET_WARNING:
		forward_message(FENGNET_SOCKET_TYPE_WARNING, false, &result);
		break;
	default:
		Fengnet::inst->fengnet_error(nullptr, "Unknown socket message type %d.",type);
		return -1;
	}
	if (more) {
		return -1;
	}
	return 1;
}

void FengnetSocket::fengnet_socket_updatetime() {
	socket_server_updatetime(SOCKET_SERVER, Fengnet::inst->fengnet_now());
}