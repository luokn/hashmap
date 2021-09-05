#include "mpalloc.h"

#include <stdio.h>
#include <stdlib.h>

#include "log.h"

#define __MP_BLOCK_DATA_SIZE (PAGE_SIZE - sizeof(struct __mp_block))

#define __mp_align_of(SIZE) (((SIZE) + (typeof(SIZE)) 0xF) & (~(typeof(SIZE)) 0xF))

#ifdef DEBUG

#define __mp_default_alloc(SIZE)                                       \
    ({                                                                 \
        void *__ptr = malloc(SIZE);                                    \
        INFO("Memory pool alloc %d bytes at %p", (int) (SIZE), __ptr); \
        __ptr;                                                         \
    })

#define __mp_default_free(PTR)              \
    do {                                    \
        INFO("Memory pool free %p", (PTR)); \
        free(PTR);                          \
    } while (0)

#else

#define __mp_default_alloc(SIZE) malloc(SIZE)
#define __mp_default_free(PTR) free(PTR);

#endif

#define __mp_free_blocks(HEAD)                                \
    do {                                                      \
        for (struct __mp_block *__block = (HEAD); __block;) { \
            struct __mp_block *__next = __block->next;        \
            __mp_default_free(__block);                       \
            __block = __next;                                 \
        }                                                     \
    } while (0)

int memory_pool_init(memory_pool_t *pool, size_t max_tries) {
    pool->__max_tries = max_tries;
    pool->__small     = NULL;
    pool->__large     = NULL;
    return 0;
}

int memory_pool_free(memory_pool_t *pool) {
    if (pool) {
        __mp_free_blocks(pool->__large);
        __mp_free_blocks(pool->__small);
    }
    return 0;
}

int memory_pool_clear(memory_pool_t *pool) {
    if (pool) {
        __mp_free_blocks(pool->__large);
        for (struct __mp_block *small = pool->__small; small; small = small->next) {
            small->used = 0;
        }
    }
    return 0;
}

int memory_pool_destroy(memory_pool_t *pool) {
    if (pool) {
        __mp_free_blocks(pool->__large);
        __mp_free_blocks(pool->__small);
        pool->__max_tries = 0;
        pool->__small     = NULL;
        pool->__large     = NULL;
    }
    return 0;
}

void *mpalloc(memory_pool_t *pool, size_t size) {
    void *ptr = NULL;
    if (pool) {
        size_t aligned_size = __mp_align_of(size);
        if (aligned_size <= __MP_BLOCK_DATA_SIZE) {
            struct __mp_block *small = pool->__small;
            for (size_t i = 0; small && i < pool->__max_tries; i++, small = small->next) {
                if (small->used + aligned_size <= __MP_BLOCK_DATA_SIZE) {
                    ptr = small->data + small->used;
                    small->used += aligned_size;
                    return ptr;
                }
            }
            small = (struct __mp_block *) __mp_default_alloc(PAGE_SIZE);
            if (small == NULL) return NULL;
            small->used   = aligned_size;
            small->next   = pool->__small;
            pool->__small = small;
            ptr           = small->data;
        } else {
            struct __mp_block *large = (struct __mp_block *) __mp_default_alloc(sizeof(struct __mp_block) + size);
            if (large == NULL) return NULL;
            large->next   = pool->__large;
            pool->__large = large;
            ptr           = large->data;
        }
    } else {
        ptr = __mp_default_alloc(size);
    }
    return ptr;
}

void mpfree(memory_pool_t *pool, void *ptr) {
    if (pool) {
        struct __mp_block *prev = NULL, *curr = pool->__large;
        while (curr) {
            if (curr->data == ptr) {
                if (prev)
                    prev->next = curr->next;
                else
                    pool->__large = curr->next;
                __mp_default_free(curr);
                break;
            }
            prev = curr;
            curr = curr->next;
        }
    } else {
        __mp_default_free(ptr);
    }
}
