#include <u_kernel/util/util.h>
#include <u_kernel/memory/u_memory.h>

uint32_t crc32_aarch64(const uint8_t* data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;

    while (len >= 4)
    {
        uint32_t word;

        word  = ((uint32_t)data[0]);
        word |= ((uint32_t)data[1] << 8);
        word |= ((uint32_t)data[2] << 16);
        word |= ((uint32_t)data[3] << 24);

        asm volatile(
            "crc32w %w0, %w0, %w1"
            : "+r"(crc)
            : "r"(word)
        );

        data += 4;
        len -= 4;
    }

    while (len--)
    {
        asm volatile(
            "crc32b %w0, %w0, %w1"
            : "+r"(crc)
            : "r"(*data++)
        );
    }

    return crc ^ 0xFFFFFFFF;
}

void write_uint16_alignment_safe(uint16_t* dst, uint16_t value){
    memcpy(dst, &value, sizeof(uint16_t));
}

void write_uint32_alignment_safe(uint32_t* dst, uint32_t value){
    memcpy(dst, &value, sizeof(uint32_t));
}

void write_uint64_alignment_safe(uint64_t* dst, uint64_t value){
    memcpy(dst, &value, sizeof(uint64_t));
}

uint16_t read_uint16_alignment_safe(uint16_t* src){
    uint16_t result;
    memcpy(&result, src , sizeof(uint16_t));
    return result;
}

uint32_t read_uint32_alignment_safe(uint32_t* src){
    uint32_t result;
    memcpy(&result, src , sizeof(uint32_t));
    return result;
}

uint64_t read_uint64_alignment_safe(uint64_t* src){
    uint64_t result;
    memcpy(&result, src , sizeof(uint64_t));
    return result;
}