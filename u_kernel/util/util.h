#ifndef UTIL_H
#define UTIL_H
#include <u_kernel/util/u_ctypes.h>

uint32_t crc32_aarch64(const uint8_t* data, size_t len);

void write_uint16_alignment_safe(uint16_t* dst, uint16_t value);
void write_uint32_alignment_safe(uint32_t* dst, uint32_t value);
void write_uint64_alignment_safe(uint64_t* dst, uint64_t value);

uint16_t read_uint16_alignment_safe(uint16_t* src);
uint32_t read_uint32_alignment_safe(uint32_t* src);
uint64_t read_uint64_alignment_safe(uint64_t* src);

#endif