#ifndef FENGNET_SOCKET_H
#define FENGNET_SOCKET_H

#include <memory>

#include "socket_server.h"
#include "fengnet_timer.h"
#include "fengnet_server.h"

#include "socket_info.h"

struct fengnet_context;

#define FENGNET_SOCKET_TYPE_DATA 1
#define FENGNET_SOCKET_TYPE_CONNECT 2
#define FENGNET_SOCKET_TYPE_CLOSE 3
#define FENGNET_SOCKET_TYPE_ACCEPT 4
#define FENGNET_SOCKET_TYPE_ERROR 5
#define FENGNET_SOCKET_TYPE_UDP 6
#define FENGNET_SOCKET_TYPE_WARNING 7

struct fengnet_socket_message {
	int type;
	int id;
	int ud;
	char* buffer;
};

class FengnetSocket: public SocketServer{
public:
	static FengnetSocket* socketInst;
public:
	FengnetSocket();
	FengnetSocket(const FengnetSocket&) = delete;
	FengnetSocket& operator=(const FengnetSocket&) = delete;
    void fengnet_socket_init();
	void fengnet_socket_exit();
	void fengnet_socket_free();
	int fengnet_socket_poll();
	void fengnet_socket_updatetime();

	int fengnet_socket_sendbuffer(struct fengnet_context *ctx, struct socket_sendbuffer *buffer);
	int fengnet_socket_sendbuffer_lowpriority(struct fengnet_context *ctx, struct socket_sendbuffer *buffer);
	int fengnet_socket_listen(struct fengnet_context *ctx, const char *host, int port, int backlog);
	int fengnet_socket_connect(struct fengnet_context *ctx, const char *host, int port);
	int fengnet_socket_bind(struct fengnet_context *ctx, int fd);
	void fengnet_socket_close(struct fengnet_context *ctx, int id);
	void fengnet_socket_shutdown(struct fengnet_context *ctx, int id);
	void fengnet_socket_start(struct fengnet_context *ctx, int id);
	void fengnet_socket_pause(struct fengnet_context *ctx, int id);
	void fengnet_socket_nodelay(struct fengnet_context *ctx, int id);

	int fengnet_socket_udp(struct fengnet_context *ctx, const char * addr, int port);
	int fengnet_socket_udp_connect(struct fengnet_context *ctx, int id, const char * addr, int port);
	int fengnet_socket_udp_sendbuffer(struct fengnet_context *ctx, const char * address, struct socket_sendbuffer *buffer);
	const char* fengnet_socket_udp_address(struct fengnet_socket_message *, int *addrsz);

	struct socket_info * fengnet_socket_info();
	
	// legacy APIs
	inline void sendbuffer_init_(struct socket_sendbuffer *buf, int id, const void *buffer, int sz);
	inline int fengnet_socket_send(struct fengnet_context *ctx, int id, void *buffer, int sz);
	inline int fengnet_socket_send_lowpriority(struct fengnet_context *ctx, int id, void *buffer, int sz);
	inline int fengnet_socket_udp_send(struct fengnet_context *ctx, int id, const char * address, const void *buffer, int sz);
private:
    socket_server* SOCKET_SERVER;
	// shared_ptr<FengnetServer> fengnetServer;
private:
	void forward_message(int type, bool padding, struct socket_message * result);
	
};

#endif