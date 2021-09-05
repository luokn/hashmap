#include "rand.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

static uint32_t __seed = 0;

void setseed(uint32_t seed) {
    __seed += seed;
}

uint32_t randu32() {
    __seed = (314159269 * __seed + 453806245) & RAND_MAX;
    return __seed;
}

uint32_t uniform(uint32_t min, uint32_t max) {
    return randu32() % (max - min + 1) + min;
}
