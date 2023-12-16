#ifndef FENGNET_MALLOC_H
#define FENGNET_MALLOC_H

#include <cstddef>

// #define fengnet_malloc malloc
// #define fengnet_calloc calloc
// #define fengnet_realloc realloc
// #define fengnet_free free
// #define fengnet_memalign memalign
// #define fengnet_aligned_alloc aligned_alloc
// #define fengnet_posix_memalign posix_memalign

// void * fengnet_malloc(size_t sz);
// void * fengnet_calloc(size_t nmemb,size_t size);
// void * fengnet_realloc(void *ptr, size_t size);
// void fengnet_free(void *ptr);
char * fengnet_strdup(const char *str);
void * fengnet_lalloc(void *ptr, size_t osize, size_t nsize);	// use for lua
// void * fengnet_memalign(size_t alignment, size_t size);
// void * fengnet_aligned_alloc(size_t alignment, size_t size);
// int fengnet_posix_memalign(void **memptr, size_t alignment, size_t size);

#endif
