#include "hash.h"

// SDBM Hash Function
unsigned int sdbm_hash(char *str) {
    unsigned int hash = 0;
    while (*str) {
        hash = (*str++) + (hash << 6) + (hash << 16) - hash;
    }
    return (hash & 0x7FFFFFFF);
}

// RS Hash Function
unsigned int rs_hash(char *str) {
    unsigned int hash = 0, a = 63689, b = 378551;
    while (*str) {
        hash = hash * a + (*str++);
        a *= b;
    }
    return (hash & 0x7FFFFFFF);
}

// JS Hash Function
unsigned int js_hash(char *str) {
    unsigned int hash = 1315423911;
    while (*str) {
        hash ^= ((hash << 5) + (*str++) + (hash >> 2));
    }
    return (hash & 0x7FFFFFFF);
}

// P. J. Weinberger Hash Function
unsigned int pjw_hash(char *str) {
    unsigned int bits_in_unigned_int = (unsigned int) (sizeof(unsigned int) * 8);
    unsigned int three_quarters      = (unsigned int) ((bits_in_unigned_int * 3) / 4);
    unsigned int one_eighth          = (unsigned int) (bits_in_unigned_int / 8);
    unsigned int high_bits           = (unsigned int) (0xFFFFFFFF) << (bits_in_unigned_int - one_eighth);
    unsigned int hash                = 0;
    unsigned int test                = 0;

    while (*str) {
        hash = (hash << one_eighth) + (*str++);
        if ((test = hash & high_bits) != 0) { hash = ((hash ^ (test >> three_quarters)) & (~high_bits)); }
    }
    return (hash & 0x7FFFFFFF);
}

// ELF Hash Function
unsigned int elf_hash(char *str) {
    unsigned int hash = 0;
    unsigned int x    = 0;

    while (*str) {
        hash = (hash << 4) + (*str++);
        if ((x = hash & 0xF0000000L) != 0) {
            hash ^= (x >> 24);
            hash &= ~x;
        }
    }
    return (hash & 0x7FFFFFFF);
}

// BKDR Hash Function
unsigned int bkdr_hash(char *str) {
    unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
    unsigned int hash = 0;
    while (*str) {
        hash = hash * seed + (*str++);
    }
    return (hash & 0x7FFFFFFF);
}

// DJB Hash Function
unsigned int djb_hash(char *str) {
    unsigned int hash = 5381;

    while (*str) {
        hash += (hash << 5) + (*str++);
    }
    return (hash & 0x7FFFFFFF);
}

// AP Hash Function
unsigned int ap_hash(char *str) {
    unsigned int hash = 0;
    int          i;
    for (i = 0; *str; i++) {
        if ((i & 1) == 0) {
            hash ^= ((hash << 7) ^ (*str++) ^ (hash >> 3));
        } else {
            hash ^= (~((hash << 11) ^ (*str++) ^ (hash >> 5)));
        }
    }
    return (hash & 0x7FFFFFFF);
}