#include "hashmap.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "hash.h"

int  __hm_resize(hashmap_t *, uint32_t capacity);
int  __hm_ensure_capacity(hashmap_t *map);
int  __hm_ensure_ownpool(hashmap_t *);
int  __hm_free_ownpool(hashmap_t *);
int  __hm_free_buckets(hashmap_t *);
int  __hm_free_entries(hashmap_t *);
int  __hm_convert_to_list(hashmap_t *, struct __hashmap_bucket *bucket);
int  __hm_convert_to_skiplist(hashmap_t *, struct __hashmap_bucket *bucket);
bool __hm_exists(hashmap_t *, void *key);
bool __hm_list_exists(hashmap_t *, struct __hashmap_bucket *bucket, void *key);
bool __hm_skiplist_exists(hashmap_t *, struct __hashmap_bucket *bucket, void *key);
int  __hm_insert(hashmap_t *, void *key, void *value, uint32_t hash, bool update);
int  __hm_list_insert(hashmap_t *, struct __hashmap_bucket *bucket, void *key, void *value, uint32_t hash, bool update);
int  __hm_skiplist_insert(hashmap_t *, struct __hashmap_bucket *bucket, void *key, void *value, uint32_t hash,
                          bool update);
int  __hm_try_list_insert(hashmap_t *, struct __hashmap_bucket *bucket, void *key, void *value, uint32_t hash,
                          bool update);
int  __hm_remove(hashmap_t *, void *key);
int  __hm_list_remove(hashmap_t *, struct __hashmap_bucket *bucket, void *key);
int  __hm_skiplist_remove(hashmap_t *, struct __hashmap_bucket *bucket, void *key);
int  __hm_try_skiplist_remove(hashmap_t *, struct __hashmap_bucket *bucket, void *key);
int  __hm_set(hashmap_t *, void *key, void *value);
int  __hm_list_set(hashmap_t *, struct __hashmap_bucket *bucket, void *key, void *value);
int  __hm_skiplist_set(hashmap_t *, struct __hashmap_bucket *bucket, void *key, void *value);
void *__hm_get(hashmap_t *map, void *key, void *default_value);
void *__hm_list_get(hashmap_t *, struct __hashmap_bucket *bucket, void *key, void *default_value);
void *__hm_skiplist_get(hashmap_t *, struct __hashmap_bucket *bucket, void *key, void *default_value);

#define __hm_set_entry(ENTRY, K, V, HASH, NEXT) \
    do {                                        \
        (ENTRY)->k    = (K);                    \
        (ENTRY)->v    = (V);                    \
        (ENTRY)->hash = (HASH);                 \
        (ENTRY)->next = (NEXT);                 \
    } while (0)

#define __hm_capacity_for(CAPACITY)          \
    ({                                       \
        uint32_t __capacity = (CAPACITY) -1; \
        __capacity |= __capacity >> 1;       \
        __capacity |= __capacity >> 2;       \
        __capacity |= __capacity >> 4;       \
        __capacity |= __capacity >> 8;       \
        __capacity |= __capacity >> 16;      \
        __capacity + 1;                      \
    })

#define __hm_bucket_for(MAP, HASH) (&(MAP)->__buckets[(HASH) & ((MAP)->__capacity - 1)])
#define __hm_alloc_skiplist(POOL) (skiplist_t *) mpalloc((POOL), sizeof(skiplist_t))
#define __hm_alloc_buckets(POOL, N) (struct __hashmap_bucket *) mpalloc((POOL), (N) * sizeof(struct __hashmap_bucket))
#define __hm_alloc_entries(POOL, N) (struct __hashmap_entry *) mpalloc((POOL), (N) * sizeof(struct __hashmap_entry))
#define __hm_load_max(CAPACITY) (((CAPACITY) >> 1) + ((CAPACITY) >> 2))

enum { __HM_EMPTY = 0, __HM_LIST = 1, __HM_SKIPLIST = 2 };

int hashmap_init(hashmap_t *map, uint32_t capacity, uint32_t (*hash)(void *), int (*equal)(void *, void *),
                 memory_pool_t *pool) {
    // Check capacity
    return_if(-1, capacity > HASHMAP_MAX_SIZE);
    capacity = capacity < HASHMAP_MIN_SIZE ? HASHMAP_MIN_SIZE : __hm_capacity_for(capacity);
    // Allocate memory
    struct __hashmap_bucket *buckets = __hm_alloc_buckets(pool, capacity);
    return_if_null(-1, buckets);
    struct __hashmap_entry *entries = __hm_alloc_entries(pool, capacity);
    return_if_null((mpfree(pool, buckets), -1), entries);
    memset(buckets, 0, capacity * sizeof(struct __hashmap_bucket));
    // Set map members
    map->__size     = 0;
    map->__capacity = capacity;
    map->__buckets  = buckets;
    map->__entries  = entries;
    map->__current  = 0;
    map->__freelist = -1;
    map->__pool     = pool;
    map->__ownpool  = NULL;
    map->__hash     = hash ? hash : cast_as(bkdr_hash, map->__hash);
    map->__equal    = equal ? equal : cast_as(strcmp, map->__equal);
    return 0;
}

int hashmap_free(hashmap_t *map) {
    __hm_free_buckets(map);
    __hm_free_entries(map);
    __hm_free_ownpool(map);
    return 0;
}

int hashmap_destroy(hashmap_t *map) {
    hashmap_free(map);
    map->__size     = 0;
    map->__capacity = 0;
    map->__current  = 0;
    map->__freelist = -1;
    map->__pool     = NULL;
    map->__hash     = NULL;
    map->__equal    = NULL;
    return 0;
}

uint32_t hashmap_size(hashmap_t *map) {
    return map->__size;
}

uint32_t hashmap_capacity(hashmap_t *map) {
    return map->__capacity;
}

bool hashmap_exists(hashmap_t *map, void *key) {
    return __hm_exists(map, key);
}

int hashmap_insert(hashmap_t *map, void *key, void *value, bool update) {
    return_if(-1, __hm_ensure_capacity(map) != 0);
    return __hm_insert(map, key, value, hashmap_hash(map, key), update);
}

int hashmap_remove(hashmap_t *map, void *key) {
    return __hm_remove(map, key);
}

int hashmap_set(hashmap_t *map, void *key, void *value) {
    return __hm_set(map, key, value);
}

void *hashmap_get(hashmap_t *map, void *key, void *default_value) {
    return __hm_get(map, key, default_value);
}

int hashmap_clear(hashmap_t *map) {
    map->__size     = 0;
    map->__current  = 0;
    map->__freelist = -1;
    __hm_free_ownpool(map);
    memset(map->__buckets, 0, map->__capacity * sizeof(struct __hashmap_bucket));
    return 0;
}

int hashmap_resize(hashmap_t *map, uint32_t capacity) {
    return_if(-1, capacity < map->__size || capacity > HASHMAP_MAX_SIZE);  // Check capacity
    return __hm_resize(map, __hm_capacity_for(capacity));
}

void hashmap_foreach(hashmap_t *map, void (*predicate)(void *, void *, void *), void *args) {
    for (uint32_t i = 0; i < map->__capacity; i++) {
        if (map->__buckets[i].type == __HM_LIST) {
            for (int32_t j = map->__buckets[i].entry; j != -1; j = map->__entries[j].next) {
                predicate(map->__entries[j].k, map->__entries[j].v, args);
            }
        } else {
            skiplist_foreach(map->__buckets[i].skiplist, predicate, args);
        }
    }
}

int __hm_resize(hashmap_t *map, uint32_t capacity) {
    hashmap_t newmap;
    int       ret = hashmap_init(&newmap, capacity, map->__hash, map->__equal, map->__pool);
    return_if(-1, ret != 0);
    for (uint32_t i = 0; i < map->__capacity; i++) {
        switch (map->__buckets[i].type) {
            case __HM_LIST: {
                for (int32_t j = map->__buckets[i].entry; j != -1; j = map->__entries[j].next) {
                    ret = __hm_insert(&newmap, map->__entries[j].k, map->__entries[j].v, map->__entries[j].hash, false);
                    return_if((hashmap_free(&newmap), -1), ret != 0);
                }
                break;
            }
            case __HM_SKIPLIST: {
                for (struct __skiplist_node *j = map->__buckets[i].skiplist->__head->forward[0]; j; j = j->forward[0]) {
                    ret = __hm_insert(&newmap, j->k, j->v, hashmap_hash(&newmap, j->k), false);
                    return_if((hashmap_free(&newmap), -1), ret != 0);
                }
                break;
            }
            default: break;
        }
    }
    hashmap_free(map);
    memcpy(map, &newmap, sizeof(newmap));
    return 0;
}

int __hm_ensure_capacity(hashmap_t *map) {
    if (map->__size > __hm_load_max(map->__capacity)) {
        return __hm_resize(map, map->__capacity << 1);
    }
    return 0;
}

int __hm_ensure_ownpool(hashmap_t *map) {
    return_if(0, map->__ownpool);
    map->__ownpool = (memory_pool_t *) mpalloc(map->__pool, sizeof(memory_pool_t));
    return_if_null(-1, map->__ownpool);
    return memory_pool_init(map->__ownpool, 8);
}

int __hm_free_ownpool(hashmap_t *map) {
    return_if_null(0, map->__ownpool);
    memory_pool_free(map->__ownpool);
    mpfree(map->__pool, map->__ownpool);
    map->__ownpool = NULL;
    return 0;
}

int __hm_free_buckets(hashmap_t *map) {
    return_if_null(0, map->__buckets);
    mpfree(map->__pool, map->__buckets);
    map->__buckets = NULL;
    return 0;
}

int __hm_free_entries(hashmap_t *map) {
    return_if_null(0, map->__entries);
    mpfree(map->__pool, map->__entries);
    map->__entries = NULL;
    return 0;
}

int __hm_convert_to_list(hashmap_t *map, struct __hashmap_bucket *bucket) {
    skiplist_t *skiplist = bucket->skiplist;
    bucket->type         = __HM_LIST;
    bucket->entry        = -1;
    bucket->skiplist     = NULL;
    for (struct __skiplist_node *i = skiplist->__head->forward[0]; i; i = i->forward[0]) {
        __hm_list_insert(map, bucket, i->k, i->v, hashmap_hash(map, i->k), false);
    }
    map->__size -= skiplist->__size;
    mpfree(map->__ownpool, skiplist);
    return 0;
}

int __hm_convert_to_skiplist(hashmap_t *map, struct __hashmap_bucket *bucket) {
    return_if(-1, __hm_ensure_ownpool(map) != 0);
    skiplist_t *skiplist = __hm_alloc_skiplist(map->__ownpool);
    return_if_null(-1, skiplist);
    return_if(-1, skiplist_init(skiplist, map->__equal, map->__ownpool) != 0);
    int32_t prev = -1;
    for (int curr = bucket->entry; curr >= 0; prev = curr, curr = map->__entries[curr].next) {
        int ret = skiplist_insert(skiplist, map->__entries[curr].k, map->__entries[curr].v, false);
        return_if((skiplist_free(skiplist), -1), ret != 0);
    }
    map->__entries[prev].next = map->__freelist;
    map->__freelist           = bucket->entry;
    bucket->type              = __HM_SKIPLIST;
    bucket->entry             = -1;
    bucket->skiplist          = skiplist;
    return 0;
}

bool __hm_exists(hashmap_t *map, void *key) {
    struct __hashmap_bucket *bucket = __hm_bucket_for(map, hashmap_hash(map, key));
    switch (bucket->type) {
        case __HM_LIST: return __hm_list_exists(map, bucket, key);
        case __HM_SKIPLIST: return __hm_skiplist_exists(map, bucket, key);
        default: return false;
    }
}

bool __hm_list_exists(hashmap_t *map, struct __hashmap_bucket *bucket, void *key) {
    for (int32_t i = bucket->entry; i >= 0; i = map->__entries[i].next) {
        return_if(true, hashmap_equal(map, map->__entries[i].k, key) == 0);
    }
    return false;
}

bool __hm_skiplist_exists(hashmap_t *map, struct __hashmap_bucket *bucket, void *key) {
    return skiplist_exists(bucket->skiplist, key);
}

int __hm_insert(hashmap_t *map, void *key, void *value, uint32_t hash, bool update) {
    struct __hashmap_bucket *bucket = __hm_bucket_for(map, hash);
    switch (bucket->type) {
        case __HM_EMPTY: {
            bucket->type     = __HM_LIST;
            bucket->entry    = -1;
            bucket->skiplist = NULL;
            return __hm_list_insert(map, bucket, key, value, hash, update);
        }
        case __HM_LIST: return __hm_try_list_insert(map, bucket, key, value, hash, update);
        case __HM_SKIPLIST: return __hm_skiplist_insert(map, bucket, key, value, hash, update);
        default: return -1;
    }
}

int __hm_list_insert(hashmap_t *map, struct __hashmap_bucket *bucket, void *key, void *value, uint32_t hash,
                     bool update) {
    int32_t entry = map->__freelist;
    if (entry >= 0) {
        map->__freelist = map->__entries[entry].next;
    } else {
        assert(map->__current < map->__capacity);
        entry = map->__current++;
    }
    __hm_set_entry(&map->__entries[entry], key, value, hash, bucket->entry);
    bucket->entry = entry;
    map->__size++;
    return 0;
}

int __hm_skiplist_insert(hashmap_t *map, struct __hashmap_bucket *bucket, void *key, void *value, uint32_t hash,
                         bool update) {
    return skiplist_insert(bucket->skiplist, key, value, update) == 0 ? (map->__size++, 0) : -1;
}

int __hm_try_list_insert(hashmap_t *map, struct __hashmap_bucket *bucket, void *key, void *value, uint32_t hash,
                         bool update) {
    uint32_t count = 0;
    for (int32_t i = bucket->entry; i >= 0; count++, i = map->__entries[i].next) {
        if (hashmap_equal(map, map->__entries[i].k, key) != 0) continue;
        return update ? (map->__entries[i].v = value, 0) : -1;  // Try update.
    }
    if (count < HASHMAP_THRESHOLD) {
        return __hm_list_insert(map, bucket, key, value, hash, update);
    }
    return_if(-1, __hm_convert_to_skiplist(map, bucket) != 0);
    return __hm_skiplist_insert(map, bucket, key, value, hash, update);
}

int __hm_remove(hashmap_t *map, void *key) {
    struct __hashmap_bucket *bucket = __hm_bucket_for(map, hashmap_hash(map, key));
    switch (bucket->type) {
        case __HM_LIST: return __hm_list_remove(map, bucket, key);
        case __HM_SKIPLIST: return __hm_try_skiplist_remove(map, bucket, key);
        default: return 0;
    }
}

int __hm_list_remove(hashmap_t *map, struct __hashmap_bucket *bucket, void *key) {
    for (int32_t prev = -1, curr = bucket->entry; curr >= 0; prev = curr, curr = map->__entries[curr].next) {
        if (hashmap_equal(map, map->__entries[curr].k, key) != 0) continue;
        if (prev == -1)
            bucket->entry = map->__entries[curr].next;
        else
            map->__entries[prev].next = map->__entries[curr].next;
        map->__entries[curr].next = map->__freelist;
        map->__freelist           = curr;
        map->__size--;
        return 0;
    }
    return -1;
}

int __hm_skiplist_remove(hashmap_t *map, struct __hashmap_bucket *bucket, void *key) {
    return skiplist_remove(bucket->skiplist, key) == 0 ? (map->__size--, 0) : -1;
}

int __hm_try_skiplist_remove(hashmap_t *map, struct __hashmap_bucket *bucket, void *key) {
    return_if(-1, __hm_skiplist_remove(map, bucket, key) != 0);
    if (skiplist_size(bucket->skiplist) <= HASHMAP_THRESHOLD) {
        __hm_convert_to_list(map, bucket);
    }
    return 0;
}

int __hm_set(hashmap_t *map, void *key, void *value) {
    struct __hashmap_bucket *bucket = __hm_bucket_for(map, hashmap_hash(map, key));
    switch (bucket->type) {
        case __HM_LIST: return __hm_list_set(map, bucket, key, value);
        case __HM_SKIPLIST: return __hm_skiplist_set(map, bucket, key, value);
        default: return -1;
    }
}

int __hm_list_set(hashmap_t *map, struct __hashmap_bucket *bucket, void *key, void *value) {
    for (int32_t i = bucket->entry; i != -1; i = map->__entries[i].next) {
        return_if((map->__entries[i].v = value, 0), hashmap_equal(map, map->__entries[i].k, key) == 0);
    }
    return -1;
}

int __hm_skiplist_set(hashmap_t *map, struct __hashmap_bucket *bucket, void *key, void *value) {
    return skiplist_set(bucket->skiplist, key, value);
}

void *__hm_get(hashmap_t *map, void *key, void *default_value) {
    struct __hashmap_bucket *bucket = __hm_bucket_for(map, hashmap_hash(map, key));
    switch (bucket->type) {
        case __HM_LIST: return __hm_list_get(map, bucket, key, default_value);
        case __HM_SKIPLIST: return __hm_skiplist_get(map, bucket, key, default_value);
        default: return default_value;
    }
}

void *__hm_list_get(hashmap_t *map, struct __hashmap_bucket *bucket, void *key, void *default_value) {
    for (int32_t i = bucket->entry; i != -1; i = map->__entries[i].next) {
        return_if(map->__entries[i].v, hashmap_equal(map, map->__entries[i].k, key) == 0);
    }
    return default_value;
}

void *__hm_skiplist_get(hashmap_t *map, struct __hashmap_bucket *bucket, void *key, void *default_value) {
    return skiplist_get(bucket->skiplist, key, default_value);
}
