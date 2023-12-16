#ifndef SOCKET_SERVER_H
#define SOCKET_SERVER_H

#define SOCKET_DATA 0
#define SOCKET_CLOSE 1
#define SOCKET_OPEN 2
#define SOCKET_ACCEPT 3
#define SOCKET_ERR 4
#define SOCKET_EXIT 5
#define SOCKET_UDP 6
#define SOCKET_WARNING 7

// Only for internal use
#define SOCKET_RST 8
#define SOCKET_MORE 9

#define MAX_INFO 128
// MAX_SOCKET will be 2^MAX_SOCKET_P
#define MAX_SOCKET_P 16
#define MAX_EVENT 64
#define MIN_READ_BUFFER 64
#define SOCKET_TYPE_INVALID 0
#define SOCKET_TYPE_RESERVE 1
#define SOCKET_TYPE_PLISTEN 2
#define SOCKET_TYPE_LISTEN 3
#define SOCKET_TYPE_CONNECTING 4
#define SOCKET_TYPE_CONNECTED 5
#define SOCKET_TYPE_HALFCLOSE_READ 6
#define SOCKET_TYPE_HALFCLOSE_WRITE 7
#define SOCKET_TYPE_PACCEPT 8
#define SOCKET_TYPE_BIND 9

#define MAX_SOCKET (1<<MAX_SOCKET_P)

#define PRIORITY_HIGH 0
#define PRIORITY_LOW 1

#define HASH_ID(id) (((unsigned)id) % MAX_SOCKET)
#define ID_TAG16(id) ((id>>MAX_SOCKET_P) & 0xffff)

#define PROTOCOL_TCP 0
#define PROTOCOL_UDP 1
#define PROTOCOL_UDPv6 2
#define PROTOCOL_UNKNOWN 255

#define UDP_ADDRESS_SIZE 19	// ipv6 128bit + port 16bit + 1 byte type

#define MAX_UDP_PACKAGE 65535

// EAGAIN and EWOULDBLOCK may be not the same value.
#if (EAGAIN != EWOULDBLOCK)
#define AGAIN_WOULDBLOCK EAGAIN : case EWOULDBLOCK
#else
#define AGAIN_WOULDBLOCK EAGAIN
#endif

#define WARNING_SIZE (1024*1024)

#define USEROBJECT ((size_t)(-1))

#include <atomic>
#include <sys/types.h>
#include <sys/socket.h>
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <string>

#include "fengnet.h"
#include "socket_poll.h"
#include "fengnet_malloc.h"
#include "spinlock.h"
#include "socket_buffer.h"

struct write_buffer {
	write_buffer * next;
	const void *buffer;
	char *ptr;
	size_t sz;
	bool userobject;
};

struct write_buffer_udp {
	write_buffer buffer;
	uint8_t udp_address[UDP_ADDRESS_SIZE];
};

struct wb_list {
	write_buffer * head;
	write_buffer * tail;
};

struct socket_stat {
	uint64_t rtime;
	uint64_t wtime;
	uint64_t read;
	uint64_t write;
};

struct socket {
	uintptr_t opaque;
	wb_list high;
	wb_list low;
	int64_t wb_size;
	socket_stat stat;
	atomic<unsigned long> sending;
	int fd;
	int id;
	atomic<int> type;
	uint8_t protocol;
	bool reading;
	bool writing;
	bool closing;
	atomic<int> udpconnecting;
	int64_t warn_size;
	union {
		int size;
		uint8_t udp_address[UDP_ADDRESS_SIZE];
	} p;
	SpinLock dw_lock;
	int dw_offset;
	const void * dw_buffer;
	size_t dw_size;
};

struct socket_object_interface {
	const void * (*buffer)(const void *);
	size_t (*size)(const void *);
	void (*free)(void *);
};

struct socket_server {
	volatile uint64_t time;
	int reserve_fd;	// for EMFILE
	int recvctrl_fd;
	int sendctrl_fd;
	int checkctrl;
	poll_fd event_fd;
	atomic<int> alloc_id;
	int event_n;
	int event_index;
	socket_object_interface soi;
	event ev[MAX_EVENT];
	struct socket slot[MAX_SOCKET];
	char buffer[MAX_INFO];
	uint8_t udpbuffer[MAX_UDP_PACKAGE];
	fd_set rfds;	// 这里明明已经用来epoll，为什么还要定义一个select要用到的fd_set
};

struct request_open {
	int id;
	int port;
	uintptr_t opaque;
	char host[1];
};

struct request_send {
	int id;
	size_t sz;
	const void * buffer;
};

struct request_send_udp {
	struct request_send send;
	uint8_t address[UDP_ADDRESS_SIZE];
};

struct request_setudp {
	int id;
	uint8_t address[UDP_ADDRESS_SIZE];
};

struct request_close {
	int id;
	int shutdown;
	uintptr_t opaque;
};

struct request_listen {
	int id;
	int fd;
	uintptr_t opaque;
	char host[1];
};

struct request_bind {
	int id;
	int fd;
	uintptr_t opaque;
};

struct request_resumepause {
	int id;
	uintptr_t opaque;
};

struct request_setopt {
	int id;
	int what;
	int value;
};

struct request_udp {
	int id;
	int fd;
	int family;
	uintptr_t opaque;
};

/*
	The first byte is TYPE

	S Start socket
	B Bind socket
	L Listen socket
	K Close socket
	O Connect to (Open)
	X Exit
	D Send package (high)
	P Send package (low)
	A Send UDP package
	T Set opt
	U Create UDP socket
	C set udp address
	Q query info
 */

struct request_package {
	uint8_t header[8];	// 6 bytes dummy
	union {
		char buffer[256];
		struct request_open open;
		struct request_send send;
		struct request_send_udp send_udp;
		struct request_close close;
		struct request_listen listen;
		struct request_bind bind;
		struct request_resumepause resumepause;
		struct request_setopt setopt;
		struct request_udp udp;
		struct request_setudp set_udp;
	} u;
	uint8_t dummy[256];
};

union sockaddr_all {
	struct sockaddr s;
	struct sockaddr_in v4;
	struct sockaddr_in6 v6;
};

struct send_object {
	const void * buffer;
	size_t sz;
	void (*free_func)(void *);
};

struct socket_message {
	int id;
	uintptr_t opaque;
	int ud;	// for accept, ud is new connection id ; for data, ud is size of data 
	char* data;
};

struct socket_lock {
	SpinLock *lock;
	int count;
};

class SocketServer
{
public:
    socket_server* socket_server_create(uint64_t time);
	void socket_server_release( socket_server *);
	void socket_server_updatetime( socket_server *, uint64_t time);
	int socket_server_poll( socket_server *,  socket_message *result, int *more);

	void socket_server_exit( socket_server *);
	void socket_server_close( socket_server *, uintptr_t opaque, int id);
	void socket_server_shutdown( socket_server *, uintptr_t opaque, int id);
	void socket_server_start( socket_server *, uintptr_t opaque, int id);
	void socket_server_pause( socket_server *, uintptr_t opaque, int id);

	// return -1 when error
	int socket_server_send( socket_server *,  socket_sendbuffer *buffer);
	int socket_server_send_lowpriority( socket_server *,  socket_sendbuffer *buffer);

	// ctrl command below returns id
	int socket_server_listen( socket_server *, uintptr_t opaque, const char * addr, int port, int backlog);
	int socket_server_connect( socket_server *, uintptr_t opaque, const char * addr, int port);
	int socket_server_bind( socket_server *, uintptr_t opaque, int fd);

	// for tcp
	void socket_server_nodelay( socket_server *, int id);
private:
    inline void clear_wb_list(wb_list *list);
	void send_request(socket_server *ss, request_package *request, char type, int len);
	int has_cmd(struct socket_server *ss);
	int ctrl_cmd(socket_server *ss, socket_message *result);
	void block_readpipe(int pipefd, void *buffer, int sz);
	int close_socket(struct socket_server *ss, struct request_close *request, struct socket_message *result);
	int bind_socket(struct socket_server *ss, struct request_bind *request, struct socket_message *result);
	int resume_socket(struct socket_server *ss, struct request_resumepause *request, struct socket_message *result);
	int pause_socket(struct socket_server *ss, struct request_resumepause *request, struct socket_message *result);
	void setopt_socket(struct socket_server *ss, struct request_setopt *request);
	int open_socket(struct socket_server *ss, struct request_open * request, struct socket_message *result);
	inline int socket_invalid(struct socket *s, int id);
	inline void socket_lock_init(struct socket *s, struct socket_lock *sl);
	inline void socket_lock(struct socket_lock *sl);
	inline int socket_trylock(struct socket_lock *sl);
	inline void socket_unlock(struct socket_lock *sl);
	inline int halfclose_read(struct socket *s);
	inline int nomore_sending_data(struct socket *s);
	inline int send_buffer_empty(struct socket *s);
	void force_close(struct socket_server *ss, struct socket *s, struct socket_lock *l, struct socket_message *result);
	void free_wb_list(struct socket_server *ss, struct wb_list *list);
	inline void write_buffer_free(struct socket_server *ss, struct write_buffer *wb);
	void free_buffer(struct socket_server *ss, struct socket_sendbuffer *buf);
	const void* clone_buffer(struct socket_sendbuffer *buf, size_t *sz);
	void close_read(struct socket_server *ss, struct socket * s, struct socket_message *result);
	inline void check_wb_list(struct wb_list *s);
	inline int enable_write(struct socket_server *ss, struct socket *s, bool enable);
	inline int enable_read(struct socket_server *ss, struct socket *s, bool enable);
	struct socket* new_fd(struct socket_server *ss, int id, int fd, int protocol, uintptr_t opaque, bool reading);
	inline void stat_read(struct socket_server *ss, struct socket *s, int n);
	inline void stat_write(struct socket_server *ss, struct socket *s, int n);
	int report_connect(struct socket_server *ss, struct socket *s, struct socket_lock *l, struct socket_message *result);
	int getname(union sockaddr_all *u, char *buffer, size_t sz);
	int report_accept(struct socket_server *ss, struct socket *s, struct socket_message *result);
	int reserve_id(struct socket_server *ss);
	void socket_keepalive(int fd);
	inline void clear_closed_event(struct socket_server *ss, struct socket_message * result, int type);
	int forward_message_tcp(struct socket_server *ss, struct socket *s, struct socket_lock *l, struct socket_message * result);
	int report_error(struct socket *s, struct socket_message *result, const char *err);
	int forward_message_udp(struct socket_server *ss, struct socket *s, struct socket_lock *l, struct socket_message * result);
	int gen_udp_address(int protocol, union sockaddr_all *sa, uint8_t * udp_address);
	int send_buffer(struct socket_server *ss, struct socket *s, struct socket_lock *l, struct socket_message *result);
	inline bool send_object_init(struct socket_server *ss, struct send_object *so, const void *object, size_t sz);
	int send_buffer_(struct socket_server *ss, struct socket *s, struct socket_lock *l, struct socket_message *result);
	inline int list_uncomplete(struct wb_list *s);
	void raise_uncomplete(struct socket * s);
	int trigger_write(struct socket_server *ss, struct request_send * request, struct socket_message *result);
	struct write_buffer* append_sendbuffer_(struct socket_server *ss, struct wb_list *s, struct request_send * request, int size);
	inline void append_sendbuffer_udp(struct socket_server *ss, struct socket *s, int priority, struct request_send * request, const uint8_t udp_address[UDP_ADDRESS_SIZE]);
	inline void append_sendbuffer(struct socket_server *ss, struct socket *s, struct request_send * request);
	inline void append_sendbuffer_low(struct socket_server *ss,struct socket *s, struct request_send * request);
	socklen_t udp_socket_address(struct socket *s, const uint8_t udp_address[UDP_ADDRESS_SIZE], union sockaddr_all *sa);
	inline void dec_sending_ref(struct socket_server *ss, int id);
	int set_udp_address(struct socket_server *ss, struct request_setudp *request, struct socket_message *result);
	void add_udp_socket(struct socket_server *ss, struct request_udp *udp);
	int send_list(struct socket_server *ss, struct socket *s, struct wb_list *list, struct socket_lock *l, struct socket_message *result);
	int send_list_tcp(struct socket_server *ss, struct socket *s, struct wb_list *list, struct socket_lock *l, struct socket_message *result);
	int send_list_udp(struct socket_server *ss, struct socket *s, struct wb_list *list, struct socket_message *result);
	void drop_udp(struct socket_server *ss, struct socket *s, struct wb_list *list, struct write_buffer *tmp);
	int close_write(struct socket_server *ss, struct socket *s, struct socket_lock *l, struct socket_message *result);
	int send_socket(struct socket_server *ss, struct request_send * request, struct socket_message *result, int priority, const uint8_t *udp_address);
	int listen_socket(struct socket_server *ss, struct request_listen * request, struct socket_message *result);
};

#endif