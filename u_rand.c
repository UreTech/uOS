#include"u_rand.h"

void srand(unsigned int s) {
    seed = s;
}

unsigned int rand() {
    seed = seed * 1664525 + 1013904223;
    return seed;
}