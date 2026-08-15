#include <arm/firmware_mailbox.h>
#include <memory/u_memory.h>
#include <u_timer.h>
uint32_t firmware_mailbox_call(uint32_t* buffer, size_t length, uint8_t channel){

    if(length > (64 * 4)){
        udbP("FIRMWARE MAILBOX ERROR: Maximum 256 byte buffer allowed!");
        return 0;
    }

    time_point start = get_now();
    while(*VIDEOCORE_MAILBOX_STATUS & VIDEOCORE_MAILBOX_FULL){
        if(tp_to_ms(get_now() - start) > 1000){
            udbP("FIRMWARE MAILBOX ERROR: Mailbox timeout!");
            return 0;
            break;
        }
    }

    unsigned int __attribute__((aligned(16))) mbox[64];
    memcpy(mbox, buffer, length);

    asm volatile("dc civac, %0" : : "r"(&mbox[0]) : "memory");
    asm volatile("dc civac, %0" : : "r"(&mbox[16]) : "memory");
    asm volatile("dc civac, %0" : : "r"(&mbox[32]) : "memory");
    asm volatile("dc civac, %0" : : "r"(&mbox[48]) : "memory");
    asm volatile("dsb sy" : : : "memory");

    uint32_t request = (uint32_t)mbox | (channel & 0b1111);
    *VIDEOCORE_MAILBOX_WRITE = request;

    start = get_now();
    while(true){
        while((*VIDEOCORE_MAILBOX_STATUS & VIDEOCORE_MAILBOX_EMPTY)){
            if(tp_to_ms(get_now() - start) > 1000){
                udbP("FIRMWARE MAILBOX ERROR: Mailbox response timeout!");
                return 0;
                break;
            }
        }

        if(*VIDEOCORE_MAILBOX_READ == request){
            return mbox[1] == VIDEOCORE_MAILBOX_RESPONSE;
        }

        if(tp_to_ms(get_now() - start) > 1000){
            udbP("FIRMWARE MAILBOX ERROR: Mailbox response timeout2!");
            return 0;
            break;
        }
    }

    return 0;
}

uint32_t firmware_get_clock_rate(uint8_t clock_id){
    uint32_t buf[9];

    buf[0] = sizeof(uint32_t) * 9; // buffer size
    buf[1] = 0ULL; // response

    buf[2] = 0x00030002; // get clock rate tag
    buf[3] = 8ULL; // response size
    buf[4] = 8ULL; // request size

    buf[5] = clock_id; // request clock id
    buf[6] = 0ULL; // response clock id
    buf[7] = 0ULL; // response clock rate

    buf[8] = 0; // last tag

    if(firmware_mailbox_call(buf,sizeof(buf), VIDECORE_CHANNEL_ID)){
        return buf[7];
    }else{
        return buf[7];
    }
}