#include "skiplist.h"

#include <string.h>

uint32_t                __skiplist_rand_level();
struct __skiplist_node* __skiplist_alloc_node(memory_pool_t* pool, void* key, void* value, uint32_t level);

int skiplist_init(skiplist_t* skiplist, int (*compare)(void*, void*), memory_pool_t* pool) {
    struct __skiplist_node* head = __skiplist_alloc_node(pool, NULL, NULL, SKIPLIST_MAX_LEVEL);
    return_if_null(-1, head);
    skiplist->__size    = 0;
    skiplist->__level   = 1;
    skiplist->__head    = head;
    skiplist->__pool    = pool;
    skiplist->__compare = compare;
    memset(head->forward, 0, SKIPLIST_MAX_LEVEL * sizeof(void*));
    return 0;
}

int skiplist_free(skiplist_t* skiplist) {
    struct __skiplist_node* node = skiplist->__head;
    while (node) {
        struct __skiplist_node* next = node->forward[0];
        mpfree(skiplist->__pool, node);
        node = next;
    }
    skiplist->__head = NULL;
    return 0;
}

uint32_t skiplist_size(skiplist_t* skiplist) {
    return skiplist->__size;
}

uint32_t skiplist_level(skiplist_t* skiplist) {
    return skiplist->__level;
}

int skiplist_destroy(skiplist_t* skiplist) {
    skiplist_free(skiplist);
    memset(skiplist, 0, sizeof(*skiplist));
    return 0;
}

bool skiplist_exists(skiplist_t* skiplist, void* k) {
    struct __skiplist_node *prev = skiplist->__head, *curr = NULL;
    for (int64_t lv = skiplist->__level - 1; lv >= 0; --lv) {
        for (curr = prev->forward[lv]; curr; prev = curr, curr = curr->forward[lv]) {
            int ret = skiplist_compare(skiplist, curr->k, k);
            if (ret < 0) continue;
            return_if(true, ret == 0);
            break;
        }
    }
    return false;
}

int skiplist_insert(skiplist_t* skiplist, void* k, void* v, bool update) {
    struct __skiplist_node* updates[SKIPLIST_MAX_LEVEL];
    struct __skiplist_node *prev = skiplist->__head, *curr = NULL;
    for (int64_t lv = skiplist->__level - 1; lv >= 0; --lv) {
        for (curr = prev->forward[lv]; curr; prev = curr, curr = curr->forward[lv]) {
            int ret = skiplist_compare(skiplist, curr->k, k);
            if (ret < 0) continue;
            if (ret == 0) {
                return update ? (curr->v = v, skiplist->__size++, 0) : -1;
            }
            break;
        }
        updates[lv] = prev;
    }
    uint32_t                level = __skiplist_rand_level();
    struct __skiplist_node* node  = __skiplist_alloc_node(skiplist->__pool, k, v, level);
    return_if_null(-1, node);
    while (skiplist->__level < node->level) {
        updates[skiplist->__level++] = skiplist->__head;
    }
    for (uint32_t lv = 0; lv < node->level; lv++) {
        node->forward[lv]        = updates[lv]->forward[lv];
        updates[lv]->forward[lv] = node;
    }
    skiplist->__size++;
    return 0;
}

int skiplist_remove(skiplist_t* skiplist, void* k) {
    struct __skiplist_node* updates[SKIPLIST_MAX_LEVEL];
    int                     ret  = -1;
    struct __skiplist_node *prev = skiplist->__head, *curr = NULL;
    for (int64_t lv = skiplist->__level - 1; lv >= 0; --lv) {
        for (curr = prev->forward[lv]; curr; prev = curr, curr = curr->forward[lv]) {
            ret = skiplist_compare(skiplist, curr->k, k);
            if (ret < 0) continue;
            break;
        }
        updates[lv] = prev;
    }
    return_if(-1, ret != 0);
    for (uint32_t lv = 0; lv < curr->level; lv++) {
        updates[lv]->forward[lv] = curr->forward[lv];
    }
    while (skiplist->__level > 1 && skiplist->__head->forward[skiplist->__level - 1] == NULL) {
        skiplist->__level--;
    }
    mpfree(skiplist->__pool, curr);
    skiplist->__size--;
    return 0;
}

void* skiplist_get(skiplist_t* skiplist, void* k, void* default_value) {
    struct __skiplist_node *prev = skiplist->__head, *curr = NULL;
    for (int64_t lv = skiplist->__level - 1; lv >= 0; --lv) {
        for (curr = prev->forward[lv]; curr; prev = curr, curr = curr->forward[lv]) {
            int ret = skiplist_compare(skiplist, curr->k, k);
            if (ret < 0) continue;
            return_if(curr->v, ret == 0);
            break;
        }
    }
    return default_value;
}

int skiplist_set(skiplist_t* skiplist, void* k, void* v) {
    struct __skiplist_node *prev = skiplist->__head, *curr = NULL;
    for (int64_t lv = skiplist->__level - 1; lv >= 0; --lv) {
        for (curr = prev->forward[lv]; curr; prev = curr, curr = curr->forward[lv]) {
            int ret = skiplist_compare(skiplist, curr->k, k);
            if (ret < 0) continue;
            return_if((curr->v = v, 0), ret == 0);
            break;
        }
    }
    return -1;
}

int skiplist_clear(skiplist_t* skiplist) {
    return_if_null(0, skiplist->__head);
    for (struct __skiplist_node *node = skiplist->__head->forward[0], *next = NULL; node; node = next) {
        next = node->forward[0];
        mpfree(skiplist->__pool, node);
    }
    skiplist->__size  = 0;
    skiplist->__level = 1;
    memset(skiplist->__head->forward, 0, SKIPLIST_MAX_LEVEL * sizeof(void*));
    return 0;
}

void skiplist_foreach(skiplist_t* skiplist, void (*predicate)(void*, void*, void*), void* args) {
    if (skiplist->__head) {
        for (struct __skiplist_node* node = skiplist->__head->forward[0]; node; node = node->forward[0]) {
            predicate(node->k, node->v, args);
        }
    }
}

uint32_t __skiplist_rand_level() {
    uint32_t lv = 0;
    do {
        lv++;
    } while (lv < SKIPLIST_MAX_LEVEL && randu32() < RAND_MAX / 2);
    return lv;
}

struct __skiplist_node* __skiplist_alloc_node(memory_pool_t* pool, void* key, void* value, uint32_t level) {
    size_t                  size = sizeof(struct __skiplist_node) + level * sizeof(void*);
    struct __skiplist_node* node = (struct __skiplist_node*) mpalloc(pool, size);
    node->k                      = key;
    node->v                      = value;
    node->level                  = level;
    return node;
}