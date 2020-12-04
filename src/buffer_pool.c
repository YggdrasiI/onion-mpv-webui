#include <stdio.h>

#include "buffer_pool.h"

buffer_pool_t *buffer_pool_new(
        size_t pool_len,
        size_t buf_len)
{
    if (buf_len == 0 ){
        fprintf(stderr,
                "WRN (buffer_pool.c): Buffer length should be > 0.");
      return NULL;
    }
    if (pool_len == 0 ){
        fprintf(stderr,
                "WRN (buffer_pool.c): Pool size should be > 0.");
      return NULL;
    }

    buffer_pool_t *ret = calloc(1, sizeof(buffer_pool_t));
    ret->pool_len = pool_len;
    ret->buf_len = buf_len;
    ret->__buffer = calloc(ret->pool_len, sizeof(void *));
    ret->__aquired = calloc(ret->pool_len, sizeof(char));
    ret->__pos = 0;

    return ret;
}

void buffer_pool_free(buffer_pool_t **ppool)
{
    buffer_pool_t *pool = *ppool;
    int i;
    for( i=0; i<pool->pool_len; ++i){
        if( pool->__aquired[i] ){
            fprintf(stderr,
                    "WRN (buffer_pool.c): Buffer still marked as used during freeing");
        }
        free(pool->__buffer[i]);
    }
    free(pool->__buffer);
    free(pool->__aquired);
    free(pool);
    *ppool = NULL;
}

char *buffer_pool_aquire(
        buffer_pool_t *pool)
{
    size_t cur_pos = pool->__pos;
    void *ret = NULL;

    if (pool->__aquired[cur_pos]) {
        // assume that no buffer is free because current
        // position should point on free position, if avail.
        // Fallback on normal malloc
        ret = malloc(pool->buf_len * sizeof(char));
    }else{
        ret = pool->__buffer[cur_pos];
        pool->__aquired[cur_pos] = 1;
        if (ret == NULL ) {
            // alloc on first usage of this position
            ret = malloc(pool->buf_len * sizeof(char));
            pool->__buffer[cur_pos] = ret;
        }
    }

    // Search free position for next call.
    size_t next_pos = pool->__pos + 1;
    const size_t pool_len = pool->pool_len;
    if (next_pos >= pool_len){
        next_pos = 0;
    }
    while( cur_pos != next_pos ){
      if ( 0 == pool->__aquired[next_pos]){
          break;
      }

      ++next_pos;
      if (next_pos >=  pool_len){
          next_pos = 0;
      }
    }
    pool->__pos = next_pos;

    return ret;
}

void buffer_pool_release(
        buffer_pool_t *pool,
        char *buf)
{
    int i;
    for( i=0; i<pool->pool_len; ++i){
        if (buf == pool->__buffer[i]){
            pool->__aquired[i] = 0;
            return;
        }
    }

    // Buffer not found. Assume it was malloced
    // by 'buffer_pool_aquire'.
    free(buf);
}
