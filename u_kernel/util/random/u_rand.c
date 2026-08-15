#include"u_rand.h"

void srand(unsigned int s) {
    seed = s;
}

unsigned int rand() {
    seed = seed * 1664525 + 1013904223;
    return seed;
}

void init_hardware_rng(){
uint32_t val;

    val = *RNG_CTRL_REGISTER;
    val &= ~RNG_CTRL_RBGEN_MASK;
    *RNG_CTRL_REGISTER = val;

    *RNG_INT_STATUS_REGISTER = 0xFFFFFFFF;

    *RBG_SOFT_RESET_REGISTER |= 0x1;
    *RNG_SOFT_RESET_REGISTER |= 0x1;
    *RNG_SOFT_RESET_REGISTER &= ~0x1;
    *RBG_SOFT_RESET_REGISTER &= ~0x1;

    val = *RNG_CTRL_REGISTER;
    val &= ~RNG_CTRL_RBGEN_MASK;
    val |= RNG_CTRL_RBGEN_ENABLE;
    *RNG_CTRL_REGISTER = val;
}

uint32_t hardware_rng32(){
    while ((*RNG_FIFO_COUNT_REGISTER & RNG_FIFO_COUNT_MASK) == 0);
    return *RNG_FIFO_DATA_REGISTER;
}

uint64_t hardware_rng64(){
    uint64_t result = hardware_rng32();
    result |= (uint64_t)hardware_rng32() << 32;
    return result;
}