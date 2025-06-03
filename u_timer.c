#include"u_timer.h"
void delay_ms(unsigned int ms) {
    unsigned int start = *SYS_TIMER_CLO;
    while ((*SYS_TIMER_CLO - start) < (ms * 1000)) {
        // Bekle: 1 ms = 1000 μs
    }
}

static inline unsigned long long read_cntvct(void) {
    unsigned long long val;
    asm volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}

static inline unsigned long long read_cntfrq(void) {
    unsigned long long val;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

time_point get_now(){
    return read_cntvct();
}

unsigned long long tp_to_ms(time_point tp){
    return tp * 1000 / read_cntfrq();
}

unsigned long long tp_to_us(time_point tp){
    return tp * 1000000 / read_cntfrq();
}
