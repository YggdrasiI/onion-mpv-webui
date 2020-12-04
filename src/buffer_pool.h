#pragma once
#define _GNU_SOURCE

/* Non-blocking ringbuffer wrapper to malloc.
 * Return pointer to buffers of fixed size,
 * 
 * If buffer element is free, normal malloc is called.
 *
 */


#include <stdlib.h>

typedef struct buffer_pool_t {
    size_t buf_len;     // size of buffer elements
    size_t pool_len;    // number of buffer elements
    void **__buffer;    // internal buffer
    char *__aquired;  // marks aquired buffers
    size_t __pos;       // start index for search of next
                        // free buffer.
} buffer_pool_t;

/**
 * @short Creates 'pool_len' buffer of size 'buf_len'.
 */
buffer_pool_t *buffer_pool_new(
        size_t pool_len,
        size_t buf_len);

void buffer_pool_free(buffer_pool_t **ppool);

char *buffer_pool_aquire(
        buffer_pool_t *pool);

void buffer_pool_release(
        buffer_pool_t *pool,
        char *pbuf);
