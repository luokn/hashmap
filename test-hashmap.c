#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../src/hash.h"
#include "../src/hashmap.h"
#include "../src/log.h"

typedef uint32_t (*hash_fn_t)(void *);
typedef int (*equal_fn_t)(void *, void *);

void test_hashmap();
void benchmark();
void print_hashmap(hashmap_t *map);

int main(int argc, char const *argv[]) {
    // test_hashmap();
    for (size_t i = 0; i < 10; i++) {
        benchmark();
        usleep(100 * 1000);
    }
    // sizeof(hashmap_t);
    return 0;
}

void print_skiplist(skiplist_t *skiplist);

uint32_t my_hash(void *p) {
    char *str = (char *) p;
    if (str[0] <= '3') return 0;
    return 1;
}

void test_hashmap() {
    char *    strs[] = {"0A", "0B", "0C", "0D", "1A", "1B", "1C", "1D", "2A", "2B",
                    "2C", "2D", "3A", "3B", "3C", "3D", "4A", "4B", "4C", "4D"};
    hashmap_t map;
    // memory_pool_t pool;
    // memory_pool_init(&pool, 8);
    setseed(time(NULL));
    hashmap_init(&map, 16, my_hash, NULL, NULL);

    for (size_t i = 0; i < sizeof(strs) / sizeof(strs[0]); i++) {
        hashmap_insert(&map, strs[i], (void *) i, true);
        INFO("Insert %s", strs[i]);
        print_hashmap(&map);
    }

    for (size_t i = 0; i < 10; i++) {
        hashmap_remove(&map, strs[i]);
        INFO("Remove %s", strs[i]);
        print_hashmap(&map);
    }

    for (size_t i = 0; i < 5; i++) {
        hashmap_insert(&map, strs[i], (void *) i, true);
        INFO("Insert %s", strs[i]);
        print_hashmap(&map);
    }

    hashmap_destroy(&map);
    // memory_pool_destroy(&pool);
}

#define N (1000 * 1024)

void benchmark() {
    printf("N = %d, ", N);
    //
    char strs[N][8];
    memset(strs, 0, sizeof(strs));
    for (size_t i = 0; i < N; i++) {
        sprintf(strs[i], "%d", (int) i);
    }
    //
    clock_t tic = clock();
    {
        hashmap_t map;
        hashmap_init(&map, 16, NULL, NULL, NULL);
        for (size_t i = 0; i < N; i++) {
            hashmap_insert(&map, strs[i], strs[i], true);
            if (i % 2) {
                hashmap_remove(&map, strs[i]);
            }
        }
        for (size_t i = 0; i < N; i++) {
            char *v = (char *) hashmap_get(&map, strs[i], NULL);
            if (i % 2) {
                if (v != NULL) printf("!!![ERROR]!!!");
            } else {
                if (strcmp(v, strs[i]) != 0) printf("!!![ERROR]!!!");
            }
        }
        hashmap_destroy(&map);
    }
    clock_t toc = clock();
    //
    double ms = 1000 * (double) (toc - tic) / CLOCKS_PER_SEC;
    printf("T = %f ms\n", ms);
    // }
}

void print_hashmap(hashmap_t *map) {
    printf(" { capacity = %u, size = %u, current = %u, freelist = [ ", map->__capacity, map->__size, map->__current);
    for (int32_t i = map->__freelist; i >= 0; i = map->__entries[i].next) printf("%d ", i);
    printf("] }\n");

    for (uint32_t bucket_at = 0; bucket_at < map->__capacity; bucket_at++) {
        printf(" \033[1;33m• [Bucket %2u]:\033[0m ", bucket_at);
        if (map->__buckets[bucket_at].type == 2) {
            printf("\n");
            print_skiplist(map->__buckets[bucket_at].skiplist);
        } else {
            printf("\033[34m[HEAD]\033[0m -> ");
            if (map->__buckets[bucket_at].type == 1) {
                for (int32_t i = map->__buckets[bucket_at].entry; i >= 0; i = map->__entries[i].next) {
                    printf("\033[30;42m[%s]\033[0m -> ", (char *) map->__entries[i].k);
                }
            }
            printf("\033[34m[NIL]\033[0m\n");
        }
    }
}

void print_skiplist(skiplist_t *skiplist) {
    for (int64_t i = skiplist->__level - 1; i >= 0; i--) {
        struct __skiplist_node *current  = skiplist->__head->forward[i];
        uint32_t                position = 0;
        printf("  \033[35m- [Level %2ld]:\033[0m ", i);
        printf("\033[34m[HEAD]\033[0m ");
        while (current) {
            char *  k = (char *) current->k;
            int64_t v = (int64_t) current->v;
            while (position++ < v) printf("--------");
            printf("-> \033[30;42m[%s]\033[0m ", k);
            current = current->forward[i];
        }
        while (position++ < 20) printf("--------");
        printf("-> \033[34m[NIL]\033[0m\n");
    }
}