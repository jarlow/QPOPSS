#pragma once
#include <random>
#include "getticks.h"

unsigned short precomputedMods[512];

void precomputeMods(int numberOfThreads){
    int c = 0;
    for (int i=0; i<512; i++){
        precomputedMods[i] = c;
        c++;
        c = c % (numberOfThreads);
    }  
}

static inline int findOwner(unsigned int key){
    return precomputedMods[key & 511];
}

unsigned int Random_Generate()
{
    unsigned int x = rand();
    unsigned int h = rand();

    return x ^ ((h & 1) << 31);
}


static inline unsigned long* seed_rand()
{
    unsigned long* seeds;
    /* seeds = (unsigned long*) ssalloc_aligned(64, 64); */
    //seeds = (unsigned long*) memalign(64, 64);
    seeds = (unsigned long*) calloc(3,64);
    seeds[0] = getticks() % 123456789;
    seeds[1] = getticks() % 362436069;
    seeds[2] = getticks() % 521288629;
    return seeds;
}

#define my_random xorshf96

static inline unsigned long
xorshf96(unsigned long* x, unsigned long* y, unsigned long* z)  //period 2^96-1
{
    unsigned long t;
    (*x) ^= (*x) << 16;
    (*x) ^= (*x) >> 5;
    (*x) ^= (*x) << 1;

    t = *x;
    (*x) = *y;
    (*y) = *z;
    (*z) = t ^ (*x) ^ (*y);

  return *z;
}