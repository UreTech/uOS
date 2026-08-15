#ifndef U_RAND_H
#define U_RAND_H

#include <u_kernel/util/u_ctypes.h>

#define RNG_BASE 0xFE104000UL

#define RNG_CTRL_REGISTER       ((volatile uint32_t*)(RNG_BASE + 0x00))
#define RNG_SOFT_RESET_REGISTER ((volatile uint32_t*)(RNG_BASE + 0x04))
#define RBG_SOFT_RESET_REGISTER ((volatile uint32_t*)(RNG_BASE + 0x08))
#define RNG_INT_STATUS_REGISTER ((volatile uint32_t*)(RNG_BASE + 0x18))
#define RNG_FIFO_DATA_REGISTER  ((volatile uint32_t*)(RNG_BASE + 0x20))
#define RNG_FIFO_COUNT_REGISTER ((volatile uint32_t*)(RNG_BASE + 0x24))

#define RNG_CTRL_RBGEN_MASK    0x00001FFF
#define RNG_CTRL_RBGEN_ENABLE  0x00000001
#define RNG_FIFO_COUNT_MASK    0x000000FF

static unsigned int seed = 1;

void srand(unsigned int s);

unsigned int rand();

void init_hardware_rng();

uint32_t hardware_rng32();
uint64_t hardware_rng64();

#endif