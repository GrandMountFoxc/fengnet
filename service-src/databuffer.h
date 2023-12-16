#ifndef FENGNET_DATABUFFER_H
#define FENGNET_DATABUFFER_H

#include <cstdlib>
#include <cstring>
#include <cassert>

#define MESSAGEPOOL 1023

struct message {
	char* buffer;
	int size;
	message* next;
};

struct databuffer {
	int header;
	int offset;
	int size;
	message* head;
	message* tail;
};

struct messagepool_list {
	messagepool_list* next;
	message pool[MESSAGEPOOL];
};

struct messagepool {
	messagepool_list* pool;
	message* freelist;
};

class DataBuffer{

};
#endif