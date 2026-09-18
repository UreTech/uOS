CFILES = $(shell find . -type f -name '*.c')
OFILES = $(patsubst ./%.c,build/objects/%.o,$(CFILES))
INCLUDEPATH = /mnt/common_disk/vsProjects/uOS_new
GCCFLAGS = -g -Wall -O0 -fPIE -ffreestanding -nostdinc -nostdlib -mcpu=cortex-a72 -Wno-address-of-packed-member -Wno-pointer-to-int-cast -Wno-attributes -Wno-unused-variable -Wno-int-conversion -Wno-int-to-pointer-cast -Wno-array-bounds -Wno-unused-but-set-variable
CFLAGS = 

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)

all: uOSkernel8.img

$(OFILES): build/objects/%.o: %.c
	@mkdir -p $(dir $@)
	aarch64-none-elf-gcc -I $(INCLUDEPATH) $(GCCFLAGS) $(CFLAGS) -c $< -o $@

BOOT_O = build/objects/boot.o

$(BOOT_O): boot.S
	@mkdir -p $(dir $@)
	aarch64-none-elf-gcc -I $(INCLUDEPATH) $(GCCFLAGS) -c $< -o $@

uOSkernel8.img: $(BOOT_O) $(OFILES)
	aarch64-none-elf-ld -nostdlib -Map=build/kernel.map $(BOOT_O) $(OFILES) -T link.ld -o build/uOSkernel8.elf
	aarch64-none-elf-objcopy -O binary build/uOSkernel8.elf build/uOSkernel8.img

clean:
	find . -type f -name '*.o' -delete && rm -f build/uOSkernel8.elf build/uOSkernel8.img
else ifeq ($(UNAME_S),Darwin)
    $(error "Unsupported platform: $(UNAME_S)")
else ifeq ($(OS),Windows_NT)
    $(error "Unsupported platform: $(UNAME_S)")
else
    $(error "Unsupported platform: $(UNAME_S)")
endif