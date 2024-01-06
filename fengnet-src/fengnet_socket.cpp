#include "fengnet_socket.h"

FengnetSocket* FengnetSocket::socketInst;
FengnetSocket::FengnetSocket(){
	socketInst = this;
	socketInst->SOCKET_SERVER = nullptr;
}

void FengnetSocket::fengnet_socket_init(){
	// fengnetServer = make_shared<FengnetServer>();
    SOCKET_SERVER = socket_server_create(FengnetTimer::timerInst->fengnet_now());
}

void FengnetSocket::fengnet_socket_exit() {
	socket_server_exit(SOCKET_SERVER);
}

void FengnetSocket::fengnet_socket_free() {
	socket_server_release(SOCKET_SERVER);
	SOCKET_SERVER = NULL;
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

int FengnetSocket::fengnet_socket_sendbuffer(struct fengnet_context *ctx, struct socket_sendbuffer *buffer) {
	return socket_server_send(SOCKET_SERVER, buffer);
}

int FengnetSocket::fengnet_socket_sendbuffer_lowpriority(struct fengnet_context *ctx, struct socket_sendbuffer *buffer) {
	return socket_server_send_lowpriority(SOCKET_SERVER, buffer);
}

int FengnetSocket::fengnet_socket_listen(struct fengnet_context *ctx, const char *host, int port, int backlog) {
	uint32_t source = FengnetServer::serverInst->fengnet_context_handle(ctx);
	return socket_server_listen(SOCKET_SERVER, source, host, port, backlog);
}

int FengnetSocket::fengnet_socket_connect(struct fengnet_context *ctx, const char *host, int port) {
	uint32_t source = FengnetServer::serverInst->fengnet_context_handle(ctx);
	return socket_server_connect(SOCKET_SERVER, source, host, port);
}

int FengnetSocket::fengnet_socket_bind(struct fengnet_context *ctx, int fd) {
	uint32_t source = FengnetServer::serverInst->fengnet_context_handle(ctx);
	return socket_server_bind(SOCKET_SERVER, source, fd);
}

void FengnetSocket::fengnet_socket_close(struct fengnet_context *ctx, int id) {
	uint32_t source = FengnetServer::serverInst->fengnet_context_handle(ctx);
	socket_server_close(SOCKET_SERVER, source, id);
}

void FengnetSocket::fengnet_socket_shutdown(struct fengnet_context *ctx, int id) {
	uint32_t source = FengnetServer::serverInst->fengnet_context_handle(ctx);
	socket_server_shutdown(SOCKET_SERVER, source, id);
}

void FengnetSocket::fengnet_socket_start(struct fengnet_context *ctx, int id) {
	uint32_t source = FengnetServer::serverInst->fengnet_context_handle(ctx);
	socket_server_start(SOCKET_SERVER, source, id);
}

void FengnetSocket::fengnet_socket_pause(struct fengnet_context *ctx, int id) {
	uint32_t source = FengnetServer::serverInst->fengnet_context_handle(ctx);
	socket_server_pause(SOCKET_SERVER, source, id);
}


void FengnetSocket::fengnet_socket_nodelay(struct fengnet_context *ctx, int id) {
	socket_server_nodelay(SOCKET_SERVER, id);
}

int FengnetSocket::fengnet_socket_udp(struct fengnet_context *ctx, const char * addr, int port) {
	uint32_t source = FengnetServer::serverInst->fengnet_context_handle(ctx);
	return socket_server_udp(SOCKET_SERVER, source, addr, port);
}

int FengnetSocket::fengnet_socket_udp_connect(struct fengnet_context *ctx, int id, const char * addr, int port) {
	return socket_server_udp_connect(SOCKET_SERVER, id, addr, port);
}

int FengnetSocket::fengnet_socket_udp_sendbuffer(struct fengnet_context *ctx, const char * address, struct socket_sendbuffer *buffer) {
	return socket_server_udp_send(SOCKET_SERVER, (const struct socket_udp_address *)address, buffer);
}

const char* FengnetSocket::fengnet_socket_udp_address(struct fengnet_socket_message *msg, int *addrsz) {
	if (msg->type != FENGNET_SOCKET_TYPE_UDP) {
		return NULL;
	}
	struct socket_message sm;
	sm.id = msg->id;
	sm.opaque = 0;
	sm.ud = msg->ud;
	sm.data = msg->buffer;
	return (const char *)socket_server_udp_address(SOCKET_SERVER, &sm, addrsz);
}

socket_info * FengnetSocket::fengnet_socket_info() {
	return socket_server_info(SOCKET_SERVER);
}

void FengnetSocket::fengnet_socket_updatetime() {
	socket_server_updatetime(SOCKET_SERVER, Fengnet::inst->fengnet_now());
}

void FengnetSocket::sendbuffer_init_(struct socket_sendbuffer *buf, int id, const void *buffer, int sz) {
	buf->id = id;
	buf->buffer = buffer;
	if (sz < 0) {
		buf->type = SOCKET_BUFFER_OBJECT;
	} else {
		buf->type = SOCKET_BUFFER_MEMORY;
	}
	buf->sz = (size_t)sz;
}

int FengnetSocket::fengnet_socket_send(struct fengnet_context *ctx, int id, void *buffer, int sz) {
	struct socket_sendbuffer tmp;
	sendbuffer_init_(&tmp, id, buffer, sz);
	return fengnet_socket_sendbuffer(ctx, &tmp);
}

int FengnetSocket::fengnet_socket_send_lowpriority(struct fengnet_context *ctx, int id, void *buffer, int sz) {
	struct socket_sendbuffer tmp;
	sendbuffer_init_(&tmp, id, buffer, sz);
	return fengnet_socket_sendbuffer_lowpriority(ctx, &tmp);
}

int FengnetSocket::fengnet_socket_udp_send(struct fengnet_context *ctx, int id, const char * address, const void *buffer, int sz) {
	struct socket_sendbuffer tmp;
	sendbuffer_init_(&tmp, id, buffer, sz);
	return fengnet_socket_udp_sendbuffer(ctx, address, &tmp);
}