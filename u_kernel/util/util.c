#include <u_kernel/util/util.h>

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