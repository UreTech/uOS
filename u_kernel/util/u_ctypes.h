typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef unsigned long long uintptr_t;
typedef long long intptr_t;
#define true 1
#define false 0
typedef uint8_t bool;
typedef int8_t s8;
typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef int64_t s64;
typedef uint64_t u64;
typedef uint64_t size_t;
#define NULL 0
#define nullptr NULL

#define PARTIAL_SUCCESS -2
#define FAIL -1
#define SUCCESS 0
typedef int uos_result;

//#define IDX_NONE ((uint32_t)-1)

#define UINT8_MAX 0xFFu
#define UINT16_MAX 0xFFFFu
#define UINT32_MAX 0xFFFFFFFFu
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFu

#define SIZE_1K (0x400ULL)
#define SIZE_1M (0x400ULL * 0x400ULL)
#define SIZE_1G (0x400ULL * 0x400ULL * 0x400ULL)
#define SIZE_1T (0x400ULL * 0x400ULL * 0x400ULL * 0x400ULL)

// util macro
#define ONEBIT(offset) ((uint64_t)(1) << offset)
#define MASK_LOW(offset) (((uint64_t)(1) << (offset + 1)) - 1)
#define CLOCK_KHZ(clk_khz) (clk_khz * 1000ULL)
#define CLOCK_MHZ(clk_mhz) (CLOCK_KHZ(clk_mhz) * 1000ULL)