#include"u_cstr_util.h"
#include"u_ctypes.h"

char ntoc(uint8_t val){
    if(val <= 9){
        return '0' + val;
    }else{
        return 'E';
    }
}

u64 pof(u64 base, u64 exponent){
    u64 result = 1;
    while(exponent--){
        result *= base;
    }
    return result;
}

size_t strlen(const char* str, size_t maxSize){
    size_t res = 0;
    while(*(str + res) != '\0'){
        if(res > maxSize) return 0;
        res++;
    }
    return res;
}

char* ulltoa(uint64_t value) {
    static char buffer[21];
    char* ptr = &buffer[20];
    *ptr = '\0';

    if (value == 0) {
        *(--ptr) = '0';
        return ptr;
    }

    while (value > 0) {
        *(--ptr) = '0' + (value % 10);
        value /= 10;
    }

    return ptr;
}

char toupper(char ch){
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 'A';
    return ch;
}
