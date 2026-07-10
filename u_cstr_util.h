#include "u_ctypes.h"

#include <memory/u_memory.h>

char ntoc(uint8_t val);

u64 pof(u64 base, u64 exponent);

char *ulltoa(uint64_t val, char *buf);

char *lltoa(int64_t val, char *buf);

char *ftoa(float val, int precision, char *buf);

char *ulltohexa(uint64_t val, char *buf);

// !CAUTION! free the output buffer when its job is done
char *append_strs(char **strs, size_t count);

size_t strlen(const char *str, size_t maxSize);

char toupper(char ch);

// be careful! can lock down the kernel!
char *strcpy(char *dst, const char *src);

// be careful! can lock down the kernel!
int strcmp(const char *str0, const char *str1);