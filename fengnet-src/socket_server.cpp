#include "socket_server.h"

void SocketServer::clear_wb_list(struct wb_list *list) {
	list->head = NULL;
	list->tail = NULL;
}

socket_server* SocketServer::socket_server_create(uint64_t time){
    int i;
    int fd[2];
    poll_fd efd = sp_create();
    if(sp_invalid(efd)){
        Fengnet::inst->fengnet_error(nullptr, "socket-server: create event pool failed.");
        return nullptr;
    }
    // int pipe(int fds[2]);
	// 参数：
	// fd[0] 将是管道读取端的fd（文件描述符）
	// fd[1] 将是管道写入端的fd
	// 返回值：0表示成功，-1表示失败。
	if (pipe(fd)) {
		sp_release(efd);
		Fengnet::inst->fengnet_error(nullptr, "socket-server: create socket pair failed.");
		return nullptr;
	}
	if (sp_add(efd, fd[0], nullptr)) {
		// add recvctrl_fd to event poll
		Fengnet::inst->fengnet_error(nullptr, "socket-server: can't add server fd to event pool.");
		close(fd[0]);
		close(fd[1]);
		sp_release(efd);
		return nullptr;
	}

    socket_server* ss = new socket_server();
    ss->time = time;
	ss->event_fd = efd;
	ss->recvctrl_fd = fd[0];
	ss->sendctrl_fd = fd[1];
	ss->checkctrl = 1;
	ss->reserve_fd = dup(1);	// reserve an extra fd for EMFILE

    for (i=0;i<MAX_SOCKET;i++) {
		struct socket *s = &ss->slot[i];
		s->type = SOCKET_TYPE_INVALID;
		clear_wb_list(&s->high);
		clear_wb_list(&s->low);
	}
    ss->alloc_id = 0;
	ss->event_n = 0;
	ss->event_index = 0;
	memset(&ss->soi, 0, sizeof(ss->soi));
	FD_ZERO(&ss->rfds);	// 清空套接字集
	assert(ss->recvctrl_fd < FD_SETSIZE);

	return ss;
}

void SocketServer::send_request(socket_server *ss, request_package *request, char type, int len) {
	request->header[6] = (uint8_t)type;
	request->header[7] = (uint8_t)len;
	const char * req = (const char *)request + offsetof(request_package, header[6]);
	for (;;) {
		ssize_t n = write(ss->sendctrl_fd, req, len+2);
		if (n<0) {
			if (errno != EINTR) {
				Fengnet::inst->fengnet_error(NULL, "socket-server : send ctrl command error %s.", strerror(errno));
			}
			continue;
		}
		assert(n == len+2);
		return;
	}
}

void SocketServer::socket_server_exit(struct socket_server *ss) {
	struct request_package request;
	send_request(ss, &request, 'X', 0);
}

int SocketServer::has_cmd(struct socket_server *ss) {
	struct timeval tv = {0,0};
	int retval;

	FD_SET(ss->recvctrl_fd, &ss->rfds);

	retval = select(ss->recvctrl_fd+1, &ss->rfds, NULL, NULL, &tv);
	if (retval == 1) {
		return 1;
	}
	return 0;
}

void SocketServer::socket_lock_init(struct socket *s, struct socket_lock *sl) {
	sl->lock = &s->dw_lock;
	sl->count = 0;
}

void SocketServer::socket_lock(struct socket_lock *sl) {
	if (sl->count == 0) {
		sl->lock->lock();
	}
	++sl->count;
}

int SocketServer::socket_trylock(struct socket_lock *sl) {
	if (sl->count == 0) {
		if (!sl->lock->trylock())
			return 0;	// lock failed
	}
	++sl->count;
	return 1;
}

void SocketServer::socket_unlock(struct socket_lock *sl) {
	--sl->count;
	if (sl->count <= 0) {
		assert(sl->count == 0);
		sl->lock->unlock();
	}
}

int SocketServer::socket_invalid(struct socket *s, int id) {
	return (s->id != id || atomic_load(&s->type) == SOCKET_TYPE_INVALID);
}

int SocketServer::send_buffer_empty(struct socket *s) {
	return (s->high.head == nullptr && s->low.head == nullptr);
}

inline int SocketServer::nomore_sending_data(struct socket *s) {
	return (send_buffer_empty(s) && s->dw_buffer == NULL && (atomic_load(&s->sending) & 0xffff) == 0)
		|| (atomic_load(&s->type) == SOCKET_TYPE_HALFCLOSE_WRITE);
}

void SocketServer::close_read(struct socket_server *ss, struct socket * s, struct socket_message *result) {
	// Don't read socket later
	atomic_store(&s->type , SOCKET_TYPE_HALFCLOSE_READ);
	enable_read(ss,s,false);
	shutdown(s->fd, SHUT_RD);
	result->id = s->id;
	result->ud = 0;
	result->data = NULL;
	result->opaque = s->opaque;
}

int SocketServer::halfclose_read(struct socket *s) {
	return atomic_load(&s->type) == SOCKET_TYPE_HALFCLOSE_READ;
}

// SOCKET_CLOSE can be raised (only once) in one of two conditions.
// See https://github.com/cloudwu/skynet/issues/1346 for more discussion.
// 1. close socket by self, See close_socket()
// 2. recv 0 or eof event (close socket by remote), See forward_message_tcp()
// It's able to write data after SOCKET_CLOSE (In condition 2), but if remote is closed, SOCKET_ERR may raised.
int SocketServer::close_socket(struct socket_server *ss, struct request_close *request, struct socket_message *result) {
	int id = request->id;
	struct socket * s = &ss->slot[HASH_ID(id)];
	if (socket_invalid(s, id)) {
		// The socket is closed, ignore
		return -1;
	}
	struct socket_lock l;
	socket_lock_init(s, &l);

	int shutdown_read = halfclose_read(s);

	if (request->shutdown || nomore_sending_data(s)) {
		// If socket is SOCKET_TYPE_HALFCLOSE_READ, Do not raise SOCKET_CLOSE again.
		int r = shutdown_read ? -1 : SOCKET_CLOSE;
		force_close(ss,s,&l,result);
		return r;
	}
	s->closing = true;
	if (!shutdown_read) {
		// don't read socket after socket.close()
		close_read(ss, s, result);
		return SOCKET_CLOSE;
	}
	// recv 0 before (socket is SOCKET_TYPE_HALFCLOSE_READ) and waiting for sending data out.
	return -1;
}

int SocketServer::bind_socket(struct socket_server *ss, struct request_bind *request, struct socket_message *result) {
	int id = request->id;
	result->id = id;
	result->opaque = request->opaque;
	result->ud = 0;
	struct socket *s = new_fd(ss, id, request->fd, PROTOCOL_TCP, request->opaque, true);
	if (s == NULL) {
		result->data = "reach skynet socket number limit";
		return SOCKET_ERR;
	}
	sp_nonblocking(request->fd);
	atomic_store(&s->type , SOCKET_TYPE_BIND);
	result->data = "binding";
	return SOCKET_OPEN;
}

int SocketServer::resume_socket(struct socket_server *ss, struct request_resumepause *request, struct socket_message *result) {
	int id = request->id;
	result->id = id;
	result->opaque = request->opaque;
	result->ud = 0;
	result->data = NULL;
	struct socket *s = &ss->slot[HASH_ID(id)];
	if (socket_invalid(s, id)) {
		result->data = "invalid socket";
		return SOCKET_ERR;
	}
	if (halfclose_read(s)) {
		// The closing socket may be in transit, so raise an error. See https://github.com/cloudwu/skynet/issues/1374
		result->data = "socket closed";
		return SOCKET_ERR;
	}
	struct socket_lock l;
	socket_lock_init(s, &l);
	if (enable_read(ss, s, true)) {
		result->data = "enable read failed";
		return SOCKET_ERR;
	}
	uint8_t type = atomic_load(&s->type);
	if (type == SOCKET_TYPE_PACCEPT || type == SOCKET_TYPE_PLISTEN) {
		atomic_store(&s->type , (type == SOCKET_TYPE_PACCEPT) ? SOCKET_TYPE_CONNECTED : SOCKET_TYPE_LISTEN);
		s->opaque = request->opaque;
		result->data = "start";
		return SOCKET_OPEN;
	} else if (type == SOCKET_TYPE_CONNECTED) {
		// todo: maybe we should send a message SOCKET_TRANSFER to s->opaque
		s->opaque = request->opaque;
		result->data = "transfer";
		return SOCKET_OPEN;
	}
	// if s->type == SOCKET_TYPE_HALFCLOSE_WRITE , SOCKET_CLOSE message will send later
	return -1;
}

int SocketServer::pause_socket(struct socket_server *ss, struct request_resumepause *request, struct socket_message *result) {
	int id = request->id;
	struct socket *s = &ss->slot[HASH_ID(id)];
	if (socket_invalid(s, id)) {
		return -1;
	}
	if (enable_read(ss, s, false)) {
		return report_error(s, result, "enable read failed");
	}
	return -1;
}

void SocketServer::setopt_socket(struct socket_server *ss, struct request_setopt *request) {
	int id = request->id;
	struct socket *s = &ss->slot[HASH_ID(id)];
	if (socket_invalid(s, id)) {
		return;
	}
	int v = request->value;
	setsockopt(s->fd, IPPROTO_TCP, request->what, &v, sizeof(v));
}

// return -1 when connecting
int SocketServer::open_socket(struct socket_server *ss, struct request_open * request, struct socket_message *result) {
	int id = request->id;
	result->opaque = request->opaque;
	result->id = id;
	result->ud = 0;
	result->data = NULL;
	struct socket *ns;
	int status;
	struct addrinfo ai_hints;
	struct addrinfo *ai_list = NULL;
	struct addrinfo *ai_ptr = NULL;
	char port[16];
	int sock= -1;
	sprintf(port, "%d", request->port);
	memset(&ai_hints, 0, sizeof( ai_hints ) );
	ai_hints.ai_family = AF_UNSPEC;
	ai_hints.ai_socktype = SOCK_STREAM;
	ai_hints.ai_protocol = IPPROTO_TCP;

	status = getaddrinfo( request->host, port, &ai_hints, &ai_list );
	if ( status != 0 ) {
		result->data = const_cast<char*>(gai_strerror(status));
		goto _failed_getaddrinfo;
	}
	for (ai_ptr = ai_list; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next ) {
		sock = socket( ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol );
		if ( sock < 0 ) {
			continue;
		}
		socket_keepalive(sock);
		sp_nonblocking(sock);
		status = connect( sock, ai_ptr->ai_addr, ai_ptr->ai_addrlen);
		if ( status != 0 && errno != EINPROGRESS) {
			close(sock);
			sock = -1;
			continue;
		}
		break;
	}

	if (sock < 0) {
		result->data = strerror(errno);
		goto _failed;
	}

	ns = new_fd(ss, id, sock, PROTOCOL_TCP, request->opaque, true);
	if (ns == NULL) {
		result->data = "reach skynet socket number limit";
		goto _failed;
	}

	if(status == 0) {
		atomic_store(&ns->type , SOCKET_TYPE_CONNECTED);
		struct sockaddr * addr = ai_ptr->ai_addr;
		void * sin_addr = (ai_ptr->ai_family == AF_INET) ? (void*)&((struct sockaddr_in *)addr)->sin_addr : (void*)&((struct sockaddr_in6 *)addr)->sin6_addr;
		if (inet_ntop(ai_ptr->ai_family, sin_addr, ss->buffer, sizeof(ss->buffer))) {
			result->data = ss->buffer;
		}
		freeaddrinfo( ai_list );
		return SOCKET_OPEN;
	} else {
		if (enable_write(ss, ns, true)) {
			result->data = "enable write failed";
			goto _failed;
		}
		atomic_store(&ns->type , SOCKET_TYPE_CONNECTING);
	}

	freeaddrinfo( ai_list );
	return -1;
_failed:
	if (sock >= 0)
		close(sock);
	freeaddrinfo( ai_list );
_failed_getaddrinfo:
	atomic_store(&ss->slot[HASH_ID(id)].type, SOCKET_TYPE_INVALID);
	return SOCKET_ERR;
}

void SocketServer::block_readpipe(int pipefd, void *buffer, int sz) {
	for (;;) {
		int n = read(pipefd, buffer, sz);
		if (n<0) {
			if (errno == EINTR)
				continue;
			Fengnet::inst->fengnet_error(nullptr, "socket-server : read pipe error %s.",strerror(errno));
			return;
		}
		// must atomic read from a pipe
		assert(n == sz);
		return;
	}
}

struct write_buffer* SocketServer::append_sendbuffer_(struct socket_server *ss, struct wb_list *s, struct request_send * request, int size) {
	struct write_buffer * buf = new write_buffer();
	struct send_object so;
	buf->userobject = send_object_init(ss, &so, request->buffer, request->sz);
	buf->ptr = (char*)so.buffer;
	buf->sz = so.sz;
	buf->buffer = request->buffer;
	buf->next = NULL;
	if (s->head == NULL) {
		s->head = s->tail = buf;
	} else {
		assert(s->tail != NULL);
		assert(s->tail->next == NULL);
		s->tail->next = buf;
		s->tail = buf;
	}
	return buf;
}

void SocketServer::append_sendbuffer_udp(struct socket_server *ss, struct socket *s, int priority, struct request_send * request, const uint8_t udp_address[UDP_ADDRESS_SIZE]) {
	struct wb_list *wl = (priority == PRIORITY_HIGH) ? &s->high : &s->low;
	struct write_buffer_udp *buf = (struct write_buffer_udp *)append_sendbuffer_(ss, wl, request, sizeof(*buf));
	memcpy(buf->udp_address, udp_address, UDP_ADDRESS_SIZE);
	s->wb_size += buf->buffer.sz;
}

void SocketServer::append_sendbuffer(struct socket_server *ss, struct socket *s, struct request_send * request) {
	struct write_buffer *buf = append_sendbuffer_(ss, &s->high, request, sizeof(*buf));
	s->wb_size += buf->sz;
}

void SocketServer::append_sendbuffer_low(struct socket_server *ss,struct socket *s, struct request_send * request) {
	struct write_buffer *buf = append_sendbuffer_(ss, &s->low, request, sizeof(*buf));
	s->wb_size += buf->sz;
}

int SocketServer::trigger_write(struct socket_server *ss, struct request_send * request, struct socket_message *result) {
	int id = request->id;
	struct socket * s = &ss->slot[HASH_ID(id)];
	if (socket_invalid(s, id))
		return -1;
	if (enable_write(ss, s, true)) {
		return report_error(s, result, "enable write failed");
	}
	return -1;
}

socklen_t SocketServer::udp_socket_address(struct socket *s, const uint8_t udp_address[UDP_ADDRESS_SIZE], union sockaddr_all *sa) {
	int type = (uint8_t)udp_address[0];
	if (type != s->protocol)
		return 0;
	uint16_t port = 0;
	memcpy(&port, udp_address+1, sizeof(uint16_t));
	switch (s->protocol) {
	case PROTOCOL_UDP:
		memset(&sa->v4, 0, sizeof(sa->v4));
		sa->s.sa_family = AF_INET;
		sa->v4.sin_port = port;
		memcpy(&sa->v4.sin_addr, udp_address + 1 + sizeof(uint16_t), sizeof(sa->v4.sin_addr));	// ipv4 address is 32 bits
		return sizeof(sa->v4);
	case PROTOCOL_UDPv6:
		memset(&sa->v6, 0, sizeof(sa->v6));
		sa->s.sa_family = AF_INET6;
		sa->v6.sin6_port = port;
		memcpy(&sa->v6.sin6_addr, udp_address + 1 + sizeof(uint16_t), sizeof(sa->v6.sin6_addr)); // ipv6 address is 128 bits
		return sizeof(sa->v6);
	}
	return 0;
}

/*
	When send a package , we can assign the priority : PRIORITY_HIGH or PRIORITY_LOW

	If socket buffer is empty, write to fd directly.
		If write a part, append the rest part to high list. (Even priority is PRIORITY_LOW)
	Else append package to high (PRIORITY_HIGH) or low (PRIORITY_LOW) list.
 */
int SocketServer::send_socket(struct socket_server *ss, struct request_send * request, struct socket_message *result, int priority, const uint8_t *udp_address) {
	int id = request->id;
	struct socket * s = &ss->slot[HASH_ID(id)];
	struct send_object so;
	send_object_init(ss, &so, request->buffer, request->sz);
	uint8_t type = atomic_load(&s->type);
	if (type == SOCKET_TYPE_INVALID || s->id != id
		|| type == SOCKET_TYPE_HALFCLOSE_WRITE
		|| type == SOCKET_TYPE_PACCEPT
		|| s->closing) {
		so.free_func((void *)request->buffer);
		return -1;
	}
	if (type == SOCKET_TYPE_PLISTEN || type == SOCKET_TYPE_LISTEN) {
		Fengnet::inst->fengnet_error(NULL, "socket-server: write to listen fd %d.", id);
		so.free_func((void *)request->buffer);
		return -1;
	}
	if (send_buffer_empty(s)) {
		if (s->protocol == PROTOCOL_TCP) {
			append_sendbuffer(ss, s, request);	// add to high priority list, even priority == PRIORITY_LOW
		} else {
			// udp
			if (udp_address == NULL) {
				udp_address = s->p.udp_address;
			}
			union sockaddr_all sa;
			socklen_t sasz = udp_socket_address(s, udp_address, &sa);
			if (sasz == 0) {
				// udp type mismatch, just drop it.
				Fengnet::inst->fengnet_error(NULL, "socket-server: udp socket (%d) type mismatch.", id);
				so.free_func((void *)request->buffer);
				return -1;
			}
			int n = sendto(s->fd, so.buffer, so.sz, 0, &sa.s, sasz);
			if (n != so.sz) {
				append_sendbuffer_udp(ss,s,priority,request,udp_address);
			} else {
				stat_write(ss,s,n);
				so.free_func((void *)request->buffer);
				return -1;
			}
		}
		if (enable_write(ss, s, true)) {
			return report_error(s, result, "enable write failed");
		}
	} else {
		if (s->protocol == PROTOCOL_TCP) {
			if (priority == PRIORITY_LOW) {
				append_sendbuffer_low(ss, s, request);
			} else {
				append_sendbuffer(ss, s, request);
			}
		} else {
			if (udp_address == NULL) {
				udp_address = s->p.udp_address;
			}
			append_sendbuffer_udp(ss,s,priority,request,udp_address);
		}
	}
	if (s->wb_size >= WARNING_SIZE && s->wb_size >= s->warn_size) {
		s->warn_size = s->warn_size == 0 ? WARNING_SIZE *2 : s->warn_size*2;
		result->opaque = s->opaque;
		result->id = s->id;
		result->ud = s->wb_size%1024 == 0 ? s->wb_size/1024 : s->wb_size/1024 + 1;
		result->data = NULL;
		return SOCKET_WARNING;
	}
	return -1;
}

int SocketServer::listen_socket(struct socket_server *ss, struct request_listen * request, struct socket_message *result) {
	int id = request->id;
	int listen_fd = request->fd;
	struct socket *s = new_fd(ss, id, listen_fd, PROTOCOL_TCP, request->opaque, false);

	union sockaddr_all u;
	socklen_t slen = sizeof(u);
	if (s == NULL) {
		goto _failed;
	}
	atomic_store(&s->type , SOCKET_TYPE_PLISTEN);
	result->opaque = request->opaque;
	result->id = id;
	result->ud = 0;
	result->data = "listen";
	if (getsockname(listen_fd, &u.s, &slen) == 0) {
		void * sin_addr = (u.s.sa_family == AF_INET) ? (void*)&u.v4.sin_addr : (void *)&u.v6.sin6_addr;
		if (inet_ntop(u.s.sa_family, sin_addr, ss->buffer, sizeof(ss->buffer)) == 0) {
			result->data = strerror(errno);
			return SOCKET_ERR;
		}
		int sin_port = ntohs((u.s.sa_family == AF_INET) ? u.v4.sin_port : u.v6.sin6_port);
		result->data = ss->buffer;
		result->ud = sin_port;
	} else {
		result->data = strerror(errno);
		return SOCKET_ERR;
	}

	return SOCKET_OPEN;
_failed:
	close(listen_fd);
	result->opaque = request->opaque;
	result->id = id;
	result->ud = 0;
	result->data = "reach skynet socket number limit";
	ss->slot[HASH_ID(id)].type = SOCKET_TYPE_INVALID;

	return SOCKET_ERR;
}

void SocketServer::dec_sending_ref(struct socket_server *ss, int id) {
	struct socket * s = &ss->slot[HASH_ID(id)];
	// Notice: udp may inc sending while type == SOCKET_TYPE_RESERVE
	if (s->id == id && s->protocol == PROTOCOL_TCP) {
		assert((atomic_load(&s->sending) & 0xffff) != 0);
		// ATOM_FDEC(&s->sending);
		s->sending.fetch_sub(1);
	}
}

int SocketServer::set_udp_address(struct socket_server *ss, struct request_setudp *request, struct socket_message *result) {
	int id = request->id;
	struct socket *s = &ss->slot[HASH_ID(id)];
	if (socket_invalid(s, id)) {
		return -1;
	}
	int type = request->address[0];
	if (type != s->protocol) {
		// protocol mismatch
		return report_error(s, result, "protocol mismatch");
	}
	if (type == PROTOCOL_UDP) {
		memcpy(s->p.udp_address, request->address, 1+2+4);	// 1 type, 2 port, 4 ipv4
	} else {
		memcpy(s->p.udp_address, request->address, 1+2+16);	// 1 type, 2 port, 16 ipv6
	}
	// ATOM_FDEC(&s->udpconnecting);
	s->udpconnecting.fetch_sub(1);
	return -1;
}

void SocketServer::add_udp_socket(struct socket_server *ss, struct request_udp *udp) {
	int id = udp->id;
	int protocol;
	if (udp->family == AF_INET6) {
		protocol = PROTOCOL_UDPv6;
	} else {
		protocol = PROTOCOL_UDP;
	}
	struct socket *ns = new_fd(ss, id, udp->fd, protocol, udp->opaque, true);
	if (ns == NULL) {
		close(udp->fd);
		ss->slot[HASH_ID(id)].type = SOCKET_TYPE_INVALID;
		return;
	}
	atomic_store(&ns->type , SOCKET_TYPE_CONNECTED);
	memset(ns->p.udp_address, 0, sizeof(ns->p.udp_address));
}

int SocketServer::ctrl_cmd(socket_server *ss, socket_message *result) {
	int fd = ss->recvctrl_fd;
	// the length of message is one byte, so 256 buffer size is enough.
	uint8_t buffer[256];
	uint8_t header[2];
	block_readpipe(fd, header, sizeof(header));
	int type = header[0];
	int len = header[1];
	block_readpipe(fd, buffer, len);
	// ctrl command only exist in local fd, so don't worry about endian.
	switch (type) {
	case 'R':
		return resume_socket(ss,(struct request_resumepause *)buffer, result);
	case 'S':
		return pause_socket(ss,(struct request_resumepause *)buffer, result);
	case 'B':
		return bind_socket(ss,(struct request_bind *)buffer, result);
	case 'L':
		return listen_socket(ss,(struct request_listen *)buffer, result);
	case 'K':
		return close_socket(ss,(struct request_close *)buffer, result);
	case 'O':
		return open_socket(ss, (struct request_open *)buffer, result);
	case 'X':
		result->opaque = 0;
		result->id = 0;
		result->ud = 0;
		result->data = NULL;
		return SOCKET_EXIT;
	case 'W':
		return trigger_write(ss, (struct request_send *)buffer, result);
	case 'D':
	case 'P': {
		int priority = (type == 'D') ? PRIORITY_HIGH : PRIORITY_LOW;
		struct request_send * request = (struct request_send *) buffer;
		int ret = send_socket(ss, request, result, priority, NULL);
		dec_sending_ref(ss, request->id);
		return ret;
	}
	case 'A': {
		struct request_send_udp * rsu = (struct request_send_udp *)buffer;
		return send_socket(ss, &rsu->send, result, PRIORITY_HIGH, rsu->address);
	}
	case 'C':
		return set_udp_address(ss, (struct request_setudp *)buffer, result);
	case 'T':
		setopt_socket(ss, (struct request_setopt *)buffer);
		return -1;
	case 'U':
		add_udp_socket(ss, (struct request_udp *)buffer);
		return -1;
	default:
		Fengnet::inst->fengnet_error(NULL, "socket-server: Unknown ctrl %c.",type);
		return -1;
	};

	return -1;
}

int SocketServer::report_connect(struct socket_server *ss, struct socket *s, struct socket_lock *l, struct socket_message *result) {
	int error;
	socklen_t len = sizeof(error);  
	int code = getsockopt(s->fd, SOL_SOCKET, SO_ERROR, &error, &len);  
	if (code < 0 || error) {  
		error = code < 0 ? errno : error;
		force_close(ss, s, l, result);
		result->data = strerror(error);
		return SOCKET_ERR;
	} else {
		atomic_store(&s->type , SOCKET_TYPE_CONNECTED);
		result->opaque = s->opaque;
		result->id = s->id;
		result->ud = 0;
		if (nomore_sending_data(s)) {
			if (enable_write(ss, s, false)) {
				force_close(ss,s,l, result);
				result->data = "disable write failed";
				return SOCKET_ERR;
			}
		}
		union sockaddr_all u;
		socklen_t slen = sizeof(u);
		if (getpeername(s->fd, &u.s, &slen) == 0) {
			void * sin_addr = (u.s.sa_family == AF_INET) ? (void*)&u.v4.sin_addr : (void *)&u.v6.sin6_addr;
			if (inet_ntop(u.s.sa_family, sin_addr, ss->buffer, sizeof(ss->buffer))) {
				result->data = ss->buffer;
				return SOCKET_OPEN;
			}
		}
		result->data = nullptr;
		return SOCKET_OPEN;
	}
}

int SocketServer::getname(union sockaddr_all *u, char *buffer, size_t sz) {
	char tmp[INET6_ADDRSTRLEN];
	void * sin_addr = (u->s.sa_family == AF_INET) ? (void*)&u->v4.sin_addr : (void *)&u->v6.sin6_addr;
	if (inet_ntop(u->s.sa_family, sin_addr, tmp, sizeof(tmp))) {
		int sin_port = ntohs((u->s.sa_family == AF_INET) ? u->v4.sin_port : u->v6.sin6_port);
		snprintf(buffer, sz, "%s:%d", tmp, sin_port);
		return 1;
	} else {
		buffer[0] = '\0';
		return 0;
	}
}

void SocketServer::socket_keepalive(int fd) {
	int keepalive = 1;
	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive , sizeof(keepalive));  
}

int SocketServer::reserve_id(struct socket_server *ss) {
	int i;
	for (i=0;i<MAX_SOCKET;i++) {
		int id = ss->alloc_id.fetch_add(1)+1;
		if (id < 0) {
			// id = ATOM_FAND(&(ss->alloc_id), 0x7fffffff) & 0x7fffffff;
			id = ss->alloc_id.fetch_and(0x7fffffff)&0x7fffffff;
		}
		struct socket *s = &ss->slot[HASH_ID(id)];
		int type_invalid = atomic_load(&s->type);
		if (type_invalid == SOCKET_TYPE_INVALID) {
			// if (ATOM_CAS(&s->type, type_invalid, SOCKET_TYPE_RESERVE)) {
			if(s->type.compare_exchange_weak(type_invalid, SOCKET_TYPE_RESERVE)) {
				s->id = id;
				s->protocol = PROTOCOL_UNKNOWN;
				// socket_server_udp_connect may inc s->udpconncting directly (from other thread, before new_fd), 
				// so reset it to 0 here rather than in new_fd.
				s->udpconnecting = 0;
				s->fd = -1;
				return id;
			} else {
				// retry
				--i;
			}
		}
	}
	return -1;
}

// return 0 when failed, or -1 when file limit
int SocketServer::report_accept(struct socket_server *ss, struct socket *s, struct socket_message *result) {
	union sockaddr_all u;
	socklen_t len = sizeof(u);
	int client_fd = accept(s->fd, &u.s, &len);
	if (client_fd < 0) {
		if (errno == EMFILE || errno == ENFILE) {
			result->opaque = s->opaque;
			result->id = s->id;
			result->ud = 0;
			result->data = strerror(errno);

			// See https://stackoverflow.com/questions/47179793/how-to-gracefully-handle-accept-giving-emfile-and-close-the-connection
			if (ss->reserve_fd >= 0) {
				close(ss->reserve_fd);
				client_fd = accept(s->fd, &u.s, &len);
				if (client_fd >= 0) {
					close(client_fd);
				}
				ss->reserve_fd = dup(1);
			}
			return -1;
		} else {
			return 0;
		}
	}
	int id = reserve_id(ss);
	if (id < 0) {
		close(client_fd);
		return 0;
	}
	socket_keepalive(client_fd);
	sp_nonblocking(client_fd);
	struct socket *ns = new_fd(ss, id, client_fd, PROTOCOL_TCP, s->opaque, false);
	if (ns == NULL) {
		close(client_fd);
		return 0;
	}
	// accept new one connection
	stat_read(ss,s,1);

	// ATOM_STORE(&ns->type , SOCKET_TYPE_PACCEPT);
	atomic_store(&ns->type, SOCKET_TYPE_PACCEPT);
	result->opaque = s->opaque;
	result->id = s->id;
	result->ud = id;
	result->data = NULL;

	if (getname(&u, ss->buffer, sizeof(ss->buffer))) {
		result->data = ss->buffer;
	}

	return 1;
}

void SocketServer::clear_closed_event(struct socket_server *ss, struct socket_message * result, int type) {
	if (type == SOCKET_CLOSE || type == SOCKET_ERR) {
		int id = result->id;
		int i;
		for (i=ss->event_index; i<ss->event_n; i++) {
			struct event *e = &ss->ev[i];
			struct socket *s = static_cast<struct socket*>(e->s);
			if (s) {
				if (socket_invalid(s, id) && s->id == id) {
					e->s = NULL;
					break;
				}
			}
		}
	}
}

int SocketServer::report_error(struct socket *s, struct socket_message *result, const char *err) {
	result->id = s->id;
	result->ud = 0;
	result->opaque = s->opaque;
	result->data = (char *)err;
	return SOCKET_ERR;
}

int SocketServer::forward_message_tcp(struct socket_server *ss, struct socket *s, struct socket_lock *l, struct socket_message * result) {
	int sz = s->p.size;
	char * buffer = new char[sz];
	int n = (int)read(s->fd, buffer, sz);
	if (n<0) {
		delete[] buffer;
		switch(errno) {
		case EINTR:
		case AGAIN_WOULDBLOCK:
			break;
		default:
			return report_error(s, result, strerror(errno));
		}
		return -1;
	}
	if (n==0) {
		delete[] buffer;
		if (s->closing) {
			// Rare case : if s->closing is true, reading event is disable, and SOCKET_CLOSE is raised.
			if (nomore_sending_data(s)) {
				force_close(ss,s,l,result);
			}
			return -1;
		}
		int t = atomic_load(&s->type);
		if (t == SOCKET_TYPE_HALFCLOSE_READ) {
			// Rare case : Already shutdown read.
			return -1;
		}
		if (t == SOCKET_TYPE_HALFCLOSE_WRITE) {
			// Remote shutdown read (write error) before.
			force_close(ss,s,l,result);
		} else {
			close_read(ss, s, result);
		}
		return SOCKET_CLOSE;
	}

	if (halfclose_read(s)) {
		// discard recv data (Rare case : if socket is HALFCLOSE_READ, reading event is disable.)
		delete[] buffer;
		return -1;
	}

	stat_read(ss,s,n);

	result->opaque = s->opaque;
	result->id = s->id;
	result->ud = n;
	result->data = buffer;

	if (n == sz) {
		s->p.size *= 2;
		return SOCKET_MORE;
	} else if (sz > MIN_READ_BUFFER && n*2 < sz) {
		s->p.size /= 2;
	}

	return SOCKET_DATA;
}

int SocketServer::gen_udp_address(int protocol, union sockaddr_all *sa, uint8_t * udp_address) {
	int addrsz = 1;
	udp_address[0] = (uint8_t)protocol;
	if (protocol == PROTOCOL_UDP) {
		memcpy(udp_address+addrsz, &sa->v4.sin_port, sizeof(sa->v4.sin_port));
		addrsz += sizeof(sa->v4.sin_port);
		memcpy(udp_address+addrsz, &sa->v4.sin_addr, sizeof(sa->v4.sin_addr));
		addrsz += sizeof(sa->v4.sin_addr);
	} else {
		memcpy(udp_address+addrsz, &sa->v6.sin6_port, sizeof(sa->v6.sin6_port));
		addrsz += sizeof(sa->v6.sin6_port);
		memcpy(udp_address+addrsz, &sa->v6.sin6_addr, sizeof(sa->v6.sin6_addr));
		addrsz += sizeof(sa->v6.sin6_addr);
	}
	return addrsz;
}

int SocketServer::forward_message_udp(struct socket_server *ss, struct socket *s, struct socket_lock *l, struct socket_message * result) {
	union sockaddr_all sa;
	socklen_t slen = sizeof(sa);
	int n = recvfrom(s->fd, ss->udpbuffer,MAX_UDP_PACKAGE,0,&sa.s,&slen);
	if (n<0) {
		switch(errno) {
		case EINTR:
		case AGAIN_WOULDBLOCK:
			return -1;
		}
		int error = errno;
		// close when error
		force_close(ss, s, l, result);
		result->data = strerror(error);
		return SOCKET_ERR;
	}
	stat_read(ss,s,n);

	uint8_t* data;
	if (slen == sizeof(sa.v4)) {
		if (s->protocol != PROTOCOL_UDP)
			return -1;
		data = static_cast<uint8_t*>(malloc(n + 1 + 2 + 4));
		gen_udp_address(PROTOCOL_UDP, &sa, data + n);
	} else {
		if (s->protocol != PROTOCOL_UDPv6)
			return -1;
		data = static_cast<uint8_t*>(malloc(n + 1 + 2 + 16));
		gen_udp_address(PROTOCOL_UDPv6, &sa, data + n);
	}
	memcpy(data, ss->udpbuffer, n);

	result->opaque = s->opaque;
	result->id = s->id;
	result->ud = n;
	result->data = (char *)data;

	return SOCKET_UDP;
}

bool SocketServer::send_object_init(struct socket_server *ss, struct send_object *so, const void *object, size_t sz) {
	if (sz == USEROBJECT) {
		so->buffer = ss->soi.buffer(object);
		so->sz = ss->soi.size(object);
		so->free_func = ss->soi.free;
		return true;
	} else {
		so->buffer = object;
		so->sz = sz;
		so->free_func = free;
		return false;
	}
}

int SocketServer::close_write(struct socket_server *ss, struct socket *s, struct socket_lock *l, struct socket_message *result) {
	if (s->closing) {
		force_close(ss,s,l,result);
		return SOCKET_RST;
	} else {
		int t = atomic_load(&s->type);
		if (t == SOCKET_TYPE_HALFCLOSE_READ) {
			// recv 0 before, ignore the error and close fd
			force_close(ss,s,l,result);
			return SOCKET_RST;
		}
		if (t == SOCKET_TYPE_HALFCLOSE_WRITE) {
			// already raise SOCKET_ERR
			return SOCKET_RST;
		}
		atomic_store(&s->type, SOCKET_TYPE_HALFCLOSE_WRITE);
		shutdown(s->fd, SHUT_WR);
		enable_write(ss, s, false);
		return report_error(s, result, strerror(errno));
	}
}

int SocketServer::send_list_tcp(struct socket_server *ss, struct socket *s, struct wb_list *list, struct socket_lock *l, struct socket_message *result) {
	while (list->head) {
		struct write_buffer * tmp = list->head;
		for (;;) {
			ssize_t sz = write(s->fd, tmp->ptr, tmp->sz);
			if (sz < 0) {
				switch(errno) {
				case EINTR:
					continue;
				case AGAIN_WOULDBLOCK:
					return -1;
				}
				return close_write(ss, s, l, result);
			}
			stat_write(ss,s,(int)sz);
			s->wb_size -= sz;
			if (sz != tmp->sz) {
				tmp->ptr += sz;
				tmp->sz -= sz;
				return -1;
			}
			break;
		}
		list->head = tmp->next;
		write_buffer_free(ss,tmp);
	}
	list->tail = NULL;

	return -1;
}

void SocketServer::drop_udp(struct socket_server *ss, struct socket *s, struct wb_list *list, struct write_buffer *tmp) {
	s->wb_size -= tmp->sz;
	list->head = tmp->next;
	if (list->head == NULL)
		list->tail = NULL;
	write_buffer_free(ss,tmp);
}

int SocketServer::send_list_udp(struct socket_server *ss, struct socket *s, struct wb_list *list, struct socket_message *result) {
	while (list->head) {
		struct write_buffer * tmp = list->head;
		struct write_buffer_udp * udp = (struct write_buffer_udp *)tmp;
		union sockaddr_all sa;
		socklen_t sasz = udp_socket_address(s, udp->udp_address, &sa);
		if (sasz == 0) {
			Fengnet::inst->fengnet_error(NULL, "socket-server : udp (%d) type mismatch.", s->id);
			drop_udp(ss, s, list, tmp);
			return -1;
		}
		int err = sendto(s->fd, tmp->ptr, tmp->sz, 0, &sa.s, sasz);
		if (err < 0) {
			switch(errno) {
			case EINTR:
			case AGAIN_WOULDBLOCK:
				return -1;
			}
			Fengnet::inst->fengnet_error(NULL, "socket-server : udp (%d) sendto error %s.",s->id, strerror(errno));
			drop_udp(ss, s, list, tmp);
			return -1;
		}
		stat_write(ss,s,tmp->sz);
		s->wb_size -= tmp->sz;
		list->head = tmp->next;
		write_buffer_free(ss,tmp);
	}
	list->tail = NULL;

	return -1;
}

int SocketServer::send_list(struct socket_server *ss, struct socket *s, struct wb_list *list, struct socket_lock *l, struct socket_message *result) {
	if (s->protocol == PROTOCOL_TCP) {
		return send_list_tcp(ss, s, list, l, result);
	} else {
		return send_list_udp(ss, s, list, result);
	}
}

int SocketServer::list_uncomplete(struct wb_list *s) {
	struct write_buffer *wb = s->head;
	if (wb == NULL)
		return 0;
	
	return (void *)wb->ptr != wb->buffer;
}

void SocketServer::raise_uncomplete(struct socket * s) {
	struct wb_list *low = &s->low;
	struct write_buffer *tmp = low->head;
	low->head = tmp->next;
	if (low->head == NULL) {
		low->tail = NULL;
	}

	// move head of low list (tmp) to the empty high list
	struct wb_list *high = &s->high;
	assert(high->head == NULL);

	tmp->next = NULL;
	high->head = high->tail = tmp;
}

/*
	Each socket has two write buffer list, high priority and low priority.

	1. send high list as far as possible.
	2. If high list is empty, try to send low list.
	3. If low list head is uncomplete (send a part before), move the head of low list to empty high list (call raise_uncomplete) .
	4. If two lists are both empty, turn off the event. (call check_close)
 */
int SocketServer::send_buffer_(struct socket_server *ss, struct socket *s, struct socket_lock *l, struct socket_message *result) {
	assert(!list_uncomplete(&s->low));
	// step 1
	int ret = send_list(ss,s,&s->high,l,result);
	if (ret != -1) {
		if (ret == SOCKET_ERR) {
			// HALFCLOSE_WRITE
			return SOCKET_ERR;
		}
		// SOCKET_RST (ignore)
		return -1;
	}
	if (s->high.head == NULL) {
		// step 2
		if (s->low.head != NULL) {
			int ret = send_list(ss,s,&s->low,l,result);
			if (ret != -1) {
				if (ret == SOCKET_ERR) {
					// HALFCLOSE_WRITE
					return SOCKET_ERR;
				}
				// SOCKET_RST (ignore)
				return -1;
			}
			// step 3
			if (list_uncomplete(&s->low)) {
				raise_uncomplete(s);
				return -1;
			}
			if (s->low.head)
				return -1;
		} 
		// step 4
		assert(send_buffer_empty(s) && s->wb_size == 0);

		if (s->closing) {
			// finish writing
			force_close(ss, s, l, result);
			return -1;
		}

		int err = enable_write(ss, s, false);

		if (err) {
			return report_error(s, result, "disable write failed");
		}

		if(s->warn_size > 0){
			s->warn_size = 0;
			result->opaque = s->opaque;
			result->id = s->id;
			result->ud = 0;
			result->data = NULL;
			return SOCKET_WARNING;
		}
	}

	return -1;
}

int SocketServer::send_buffer(struct socket_server *ss, struct socket *s, struct socket_lock *l, struct socket_message *result) {
	if (!socket_trylock(l))
		return -1;	// blocked by direct write, send later.
	if (s->dw_buffer) {
		// add direct write buffer before high.head
		struct write_buffer * buf = new write_buffer();
		struct send_object so;
		buf->userobject = send_object_init(ss, &so, (void *)s->dw_buffer, s->dw_size);
		buf->ptr = (char*)so.buffer+s->dw_offset;
		buf->sz = so.sz - s->dw_offset;
		buf->buffer = (void *)s->dw_buffer;
		s->wb_size+=buf->sz;
		if (s->high.head == NULL) {
			s->high.head = s->high.tail = buf;
			buf->next = NULL;
		} else {
			buf->next = s->high.head;
			s->high.head = buf;
		}
		s->dw_buffer = NULL;
	}
	int r = send_buffer_(ss,s,l,result);
	socket_unlock(l);

	return r;
}

// return type
int SocketServer::socket_server_poll(struct socket_server *ss, struct socket_message * result, int * more) {
	for (;;) {
		if (ss->checkctrl) {
			if (has_cmd(ss)) {
				int type = ctrl_cmd(ss, result);
				if (type != -1) {
					clear_closed_event(ss, result, type);
					return type;
				} else
					continue;
			} else {
				ss->checkctrl = 0;
			}
		}
		if (ss->event_index == ss->event_n) {
			ss->event_n = sp_wait(ss->event_fd, ss->ev, MAX_EVENT);
			ss->checkctrl = 1;
			if (more) {
				*more = 0;
			}
			ss->event_index = 0;
			if (ss->event_n <= 0) {
				ss->event_n = 0;
				int err = errno;
				if (err != EINTR) {
					Fengnet::inst->fengnet_error(NULL, "socket-server: %s", strerror(err));
				}
				continue;
			}
		}
		struct event *e = &ss->ev[ss->event_index++];
		struct socket *s = static_cast<struct socket*>(e->s);
		if (s == NULL) {
			// dispatch pipe message at beginning
			continue;
		}
		struct socket_lock l;
		socket_lock_init(s, &l);
		switch (atomic_load(&s->type)) {
		case SOCKET_TYPE_CONNECTING:
			return report_connect(ss, s, &l, result);
		case SOCKET_TYPE_LISTEN: {
			int ok = report_accept(ss, s, result);
			if (ok > 0) {
				return SOCKET_ACCEPT;
			} if (ok < 0 ) {
				return SOCKET_ERR;
			}
			// when ok == 0, retry
			break;
		}
		case SOCKET_TYPE_INVALID:
			Fengnet::inst->fengnet_error(NULL, "socket-server: invalid socket");
			break;
		default:
			if (e->read) {
				int type;
				if (s->protocol == PROTOCOL_TCP) {
					type = forward_message_tcp(ss, s, &l, result);
					if (type == SOCKET_MORE) {
						--ss->event_index;
						return SOCKET_DATA;
					}
				} else {
					type = forward_message_udp(ss, s, &l, result);
					if (type == SOCKET_UDP) {
						// try read again
						--ss->event_index;
						return SOCKET_UDP;
					}
				}
				if (e->write && type != SOCKET_CLOSE && type != SOCKET_ERR) {
					// Try to dispatch write message next step if write flag set.
					e->read = false;
					--ss->event_index;
				}
				if (type == -1)
					break;				
				return type;
			}
			if (e->write) {
				int type = send_buffer(ss, s, &l, result);
				if (type == -1)
					break;
				return type;
			}
			if (e->error) {
				int error;
				socklen_t len = sizeof(error);  
				int code = getsockopt(s->fd, SOL_SOCKET, SO_ERROR, &error, &len);  
				const char * err = NULL;
				if (code < 0) {
					err = strerror(errno);
				} else if (error != 0) {
					err = strerror(error);
				} else {
					err = "Unknown error";
				}
				return report_error(s, result, err);
			}
			if (e->eof) {
				// For epoll (at least), FIN packets are exchanged both ways.
				// See: https://stackoverflow.com/questions/52976152/tcp-when-is-epollhup-generated
				int halfclose = halfclose_read(s);
				force_close(ss, s, &l, result);
				if (!halfclose) {
					return SOCKET_CLOSE;
				}
			}
			break;
		}
	}
}

void SocketServer::socket_server_updatetime(socket_server *ss, uint64_t time) {
	ss->time = time;
}

inline void SocketServer::write_buffer_free(struct socket_server *ss, struct write_buffer *wb) {
	if (wb->userobject) {
		ss->soi.free((void *)wb->buffer);
	} else {
		delete wb->buffer;
	}
	delete wb;
}

void SocketServer::free_wb_list(struct socket_server *ss, struct wb_list *list) {
	struct write_buffer *wb = list->head;
	while (wb) {
		struct write_buffer *tmp = wb;
		wb = wb->next;
		write_buffer_free(ss, tmp);
	}
	list->head = nullptr;
	list->tail = nullptr;
}

void SocketServer::free_buffer(struct socket_server *ss, struct socket_sendbuffer *buf) {
	void *buffer = (void *)buf->buffer;
	switch (buf->type) {
	case SOCKET_BUFFER_MEMORY:
		delete buffer;
		break;
	case SOCKET_BUFFER_OBJECT:
		ss->soi.free(buffer);
		break;
	case SOCKET_BUFFER_RAWPOINTER:
		break;
	}
}

const void* SocketServer::clone_buffer(struct socket_sendbuffer *buf, size_t *sz) {
	switch (buf->type) {
	case SOCKET_BUFFER_MEMORY:
		*sz = buf->sz;
		return buf->buffer;
	case SOCKET_BUFFER_OBJECT:
		*sz = USEROBJECT;
		return buf->buffer;
	case SOCKET_BUFFER_RAWPOINTER:
		// It's a raw pointer, we need make a copy
		*sz = buf->sz;
		void * tmp = malloc(*sz);
		memcpy(tmp, buf->buffer, *sz);
		return tmp;
	}
	// never get here
	*sz = 0;
	return nullptr;
}

void SocketServer::force_close(struct socket_server *ss, struct socket *s, struct socket_lock *l, struct socket_message *result) {
	result->id = s->id;
	result->ud = 0;
	result->data = NULL;
	result->opaque = s->opaque;
	uint8_t type = atomic_load(&s->type);
	if (type == SOCKET_TYPE_INVALID) {
		return;
	}
	assert(type != SOCKET_TYPE_RESERVE);
	free_wb_list(ss,&s->high);
	free_wb_list(ss,&s->low);
	sp_del(ss->event_fd, s->fd);
	socket_lock(l);
	if (type != SOCKET_TYPE_BIND) {
		if (close(s->fd) < 0) {
			perror("close socket:");
		}
	}
	atomic_store(&s->type, SOCKET_TYPE_INVALID);
	if (s->dw_buffer) {
		struct socket_sendbuffer tmp;
		tmp.buffer = s->dw_buffer;
		tmp.sz = s->dw_size;
		tmp.id = s->id;
		tmp.type = (tmp.sz == USEROBJECT) ? SOCKET_BUFFER_OBJECT : SOCKET_BUFFER_MEMORY;
		free_buffer(ss, &tmp);
		s->dw_buffer = NULL;
	}
	socket_unlock(l);
}

void SocketServer::socket_server_release(struct socket_server *ss) {
	int i;
	struct socket_message dummy;
	for (i=0;i<MAX_SOCKET;i++) {
		struct socket *s = &ss->slot[i];
		struct socket_lock l;
		socket_lock_init(s, &l);
		if (atomic_load(&s->type) != SOCKET_TYPE_RESERVE) {
			force_close(ss, s, &l, &dummy);
		}
		// spinlock_destroy(&s->dw_lock);
	}
	close(ss->sendctrl_fd);
	close(ss->recvctrl_fd);
	sp_release(ss->event_fd);
	if (ss->reserve_fd >= 0)
		close(ss->reserve_fd);
	delete ss;
}

void SocketServer::check_wb_list(struct wb_list *s) {
	assert(s->head == nullptr);
	assert(s->tail == nullptr);
}

int SocketServer::enable_write(struct socket_server *ss, struct socket *s, bool enable) {
	if (s->writing != enable) {
		s->writing = enable;
		return sp_enable(ss->event_fd, s->fd, s, s->reading, enable);
	}
	return 0;
}

int SocketServer::enable_read(struct socket_server *ss, struct socket *s, bool enable) {
	if (s->reading != enable) {
		s->reading = enable;
		return sp_enable(ss->event_fd, s->fd, s, enable, s->writing);
	}
	return 0;
}

struct socket* SocketServer::new_fd(struct socket_server *ss, int id, int fd, int protocol, uintptr_t opaque, bool reading) {
	struct socket * s = &ss->slot[HASH_ID(id)];
	assert(atomic_load(&s->type) == SOCKET_TYPE_RESERVE);

	if (sp_add(ss->event_fd, fd, s)) {
		atomic_store(&s->type, SOCKET_TYPE_INVALID);
		return nullptr;
	}

	s->id = id;
	s->fd = fd;
	s->reading = true;
	s->writing = false;
	s->closing = false;
	s->sending = ID_TAG16(id) << 16 | 0;
	s->protocol = protocol;
	s->p.size = MIN_READ_BUFFER;
	s->opaque = opaque;
	s->wb_size = 0;
	s->warn_size = 0;
	check_wb_list(&s->high);
	check_wb_list(&s->low);
	s->dw_buffer = nullptr;
	s->dw_size = 0;
	memset(&s->stat, 0, sizeof(s->stat));
	if (enable_read(ss, s, reading)) {
		atomic_store(&s->type , SOCKET_TYPE_INVALID);
		return nullptr;
	}
	return s;
}

void SocketServer::stat_read(struct socket_server *ss, struct socket *s, int n) {
	s->stat.read += n;
	s->stat.rtime = ss->time;
}

void SocketServer::stat_write(struct socket_server *ss, struct socket *s, int n) {
	s->stat.write += n;
	s->stat.wtime = ss->time;
}