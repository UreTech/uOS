# Details

Date : 2026-08-16 22:18:15

Directory /mnt/common_disk/vsProjects/uOS_new

Total : 55 files,  8371 codes, 2235 comments, 1640 blanks, all 12246 lines

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)

## Files
| filename | language | code | comment | blank | total |
| :--- | :--- | ---: | ---: | ---: | ---: |
| [Makefile](/Makefile) | Makefile | 34 | 0 | 11 | 45 |
| [build/kernel.map](/build/kernel.map) | Linker Map file | 746 | 0 | 27 | 773 |
| [kernel.c](/kernel.c) | C | 231 | 85 | 87 | 403 |
| [link.ld](/link.ld) | LinkerScript | 80 | 12 | 31 | 123 |
| [u\_kernel/arch/arm/u\_arm.c](/u_kernel/arch/arm/u_arm.c) | C | 409 | 4 | 27 | 440 |
| [u\_kernel/arch/arm/u\_arm.h](/u_kernel/arch/arm/u_arm.h) | C++ | 8 | 8 | 7 | 23 |
| [u\_kernel/drivers/bcm2711/firmware\_mailbox.c](/u_kernel/drivers/bcm2711/firmware_mailbox.c) | C | 62 | 0 | 15 | 77 |
| [u\_kernel/drivers/bcm2711/firmware\_mailbox.h](/u_kernel/drivers/bcm2711/firmware_mailbox.h) | C++ | 28 | 0 | 8 | 36 |
| [u\_kernel/drivers/emmc/u\_emmc.c](/u_kernel/drivers/emmc/u_emmc.c) | C | 645 | 70 | 143 | 858 |
| [u\_kernel/drivers/emmc/u\_emmc.h](/u_kernel/drivers/emmc/u_emmc.h) | C | 124 | 23 | 34 | 181 |
| [u\_kernel/drivers/gpio/u\_gpio.c](/u_kernel/drivers/gpio/u_gpio.c) | C | 105 | 0 | 15 | 120 |
| [u\_kernel/drivers/gpio/u\_gpio.h](/u_kernel/drivers/gpio/u_gpio.h) | C | 40 | 2 | 14 | 56 |
| [u\_kernel/drivers/uart/u\_uart.c](/u_kernel/drivers/uart/u_uart.c) | C | 153 | 5 | 27 | 185 |
| [u\_kernel/drivers/uart/u\_uart.h](/u_kernel/drivers/uart/u_uart.h) | C | 52 | 8 | 19 | 79 |
| [u\_kernel/drivers/udisp2/u\_display.c](/u_kernel/drivers/udisp2/u_display.c) | C | 0 | 227 | 0 | 227 |
| [u\_kernel/drivers/udisp2/u\_display.h](/u_kernel/drivers/udisp2/u_display.h) | C | 34 | 4 | 19 | 57 |
| [u\_kernel/filesystem/fat/fat.h](/u_kernel/filesystem/fat/fat.h) | C++ | 3 | 0 | 3 | 6 |
| [u\_kernel/filesystem/ufs/ufs.c](/u_kernel/filesystem/ufs/ufs.c) | C | 230 | 24 | 50 | 304 |
| [u\_kernel/filesystem/ufs/ufs.h](/u_kernel/filesystem/ufs/ufs.h) | C++ | 10 | 0 | 4 | 14 |
| [u\_kernel/filesystem/vfs/vfs.c](/u_kernel/filesystem/vfs/vfs.c) | C | 196 | 10 | 34 | 240 |
| [u\_kernel/filesystem/vfs/vfs.h](/u_kernel/filesystem/vfs/vfs.h) | C++ | 32 | 1 | 10 | 43 |
| [u\_kernel/framework/SupervisorRequestHandler/u\_SuperRH.c](/u_kernel/framework/SupervisorRequestHandler/u_SuperRH.c) | C | 128 | 20 | 26 | 174 |
| [u\_kernel/framework/SupervisorRequestHandler/u\_SuperRH.h](/u_kernel/framework/SupervisorRequestHandler/u_SuperRH.h) | C++ | 22 | 6 | 10 | 38 |
| [u\_kernel/framework/interrupt/u\_interrupt.c](/u_kernel/framework/interrupt/u_interrupt.c) | C | 269 | 54 | 58 | 381 |
| [u\_kernel/framework/interrupt/u\_interrupt.h](/u_kernel/framework/interrupt/u_interrupt.h) | C++ | 15 | 0 | 12 | 27 |
| [u\_kernel/framework/threading/u\_thread.c](/u_kernel/framework/threading/u_thread.c) | C | 347 | 94 | 92 | 533 |
| [u\_kernel/framework/threading/u\_thread.h](/u_kernel/framework/threading/u_thread.h) | C++ | 25 | 4 | 14 | 43 |
| [u\_kernel/memory/u\_heap.c](/u_kernel/memory/u_heap.c) | C | 247 | 14 | 42 | 303 |
| [u\_kernel/memory/u\_heap.h](/u_kernel/memory/u_heap.h) | C++ | 36 | 5 | 21 | 62 |
| [u\_kernel/memory/u\_memory.c](/u_kernel/memory/u_memory.c) | C | 244 | 11 | 38 | 293 |
| [u\_kernel/memory/u\_memory.h](/u_kernel/memory/u_memory.h) | C++ | 19 | 6 | 9 | 34 |
| [u\_kernel/memory/u\_mempage.c](/u_kernel/memory/u_mempage.c) | C | 287 | 36 | 71 | 394 |
| [u\_kernel/memory/u\_mempage.h](/u_kernel/memory/u_mempage.h) | C | 48 | 23 | 23 | 94 |
| [u\_kernel/memory/u\_mmu.c](/u_kernel/memory/u_mmu.c) | C | 542 | 130 | 110 | 782 |
| [u\_kernel/memory/u\_mmu.h](/u_kernel/memory/u_mmu.h) | C++ | 82 | 8 | 35 | 125 |
| [u\_kernel/objects/udevice/udevice.c](/u_kernel/objects/udevice/udevice.c) | C | 1 | 0 | 3 | 4 |
| [u\_kernel/objects/udevice/udevice.h](/u_kernel/objects/udevice/udevice.h) | C++ | 74 | 9 | 25 | 108 |
| [u\_kernel/objects/uobject.c](/u_kernel/objects/uobject.c) | C | 82 | 5 | 27 | 114 |
| [u\_kernel/objects/uobject.h](/u_kernel/objects/uobject.h) | C++ | 36 | 5 | 14 | 55 |
| [u\_kernel/timer/u\_timer.c](/u_kernel/timer/u_timer.c) | C | 52 | 7 | 9 | 68 |
| [u\_kernel/timer/u\_timer.h](/u_kernel/timer/u_timer.h) | C | 10 | 1 | 8 | 19 |
| [u\_kernel/uFX/embeded\_fonts/u\_kernelBaseFont.c](/u_kernel/uFX/embeded_fonts/u_kernelBaseFont.c) | C | 2,050 | 812 | 257 | 3,119 |
| [u\_kernel/uFX/embeded\_fonts/u\_kernelBaseFont.h](/u_kernel/uFX/embeded_fonts/u_kernelBaseFont.h) | C | 9 | 0 | 7 | 16 |
| [u\_kernel/uFX/sRND/canvas/u\_canvas.c](/u_kernel/uFX/sRND/canvas/u_canvas.c) | C | 0 | 450 | 0 | 450 |
| [u\_kernel/uFX/sRND/canvas/u\_canvas.h](/u_kernel/uFX/sRND/canvas/u_canvas.h) | C++ | 45 | 12 | 21 | 78 |
| [u\_kernel/uFX/u\_uFX.c](/u_kernel/uFX/u_uFX.c) | C | 2 | 1 | 2 | 5 |
| [u\_kernel/uFX/u\_uFX.h](/u_kernel/uFX/u_uFX.h) | C++ | 5 | 5 | 6 | 16 |
| [u\_kernel/util/lock/u\_mutex.h](/u_kernel/util/lock/u_mutex.h) | C++ | 118 | 27 | 31 | 176 |
| [u\_kernel/util/random/u\_rand.c](/u_kernel/util/random/u_rand.c) | C | 32 | 0 | 9 | 41 |
| [u\_kernel/util/random/u\_rand.h](/u_kernel/util/random/u_rand.h) | C | 20 | 0 | 10 | 30 |
| [u\_kernel/util/u\_cstr\_util.c](/u_kernel/util/u_cstr_util.c) | C | 215 | 2 | 39 | 256 |
| [u\_kernel/util/u\_cstr\_util.h](/u_kernel/util/u_cstr_util.h) | C | 14 | 3 | 12 | 29 |
| [u\_kernel/util/u\_ctypes.h](/u_kernel/util/u_ctypes.h) | C | 39 | 2 | 5 | 46 |
| [u\_kernel/util/util.c](/u_kernel/util/util.c) | C | 29 | 0 | 7 | 36 |
| [u\_kernel/util/util.h](/u_kernel/util/util.h) | C++ | 5 | 0 | 2 | 7 |

[Summary](results.md) / Details / [Diff Summary](diff.md) / [Diff Details](diff-details.md)