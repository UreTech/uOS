CFILES = $(shell find . -type f -name '*.c')
OFILES = $(CFILES:.c=.o)
INCLUDEPATH = /mnt/common_disk/vsProjects/uOS_new
GCCFLAGS = -g -Wall -O0 -fPIE -ffreestanding -nostdinc -nostdlib -mcpu=cortex-a72 -Wno-pointer-to-int-cast -Wno-attributes -Wno-unused-variable -Wno-int-conversion -Wno-int-to-pointer-cast -Wno-array-bounds -Wno-unused-but-set-variable
CFLAGS = 

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)

all: clean uOSkernel8.img

%.o: %.c
	"aarch64-none-elf-gcc" -I $(INCLUDEPATH) $(GCCFLAGS) $(CFLAGS) -c $< -o $@

boot.o: boot.S
	"aarch64-none-elf-gcc" -I $(INCLUDEPATH) $(GCCFLAGS) -c boot.S -o boot.o

uOSkernel8.img: boot.o $(OFILES)
	"aarch64-none-elf-ld" -nostdlib -Map=build/kernel.map boot.o $(OFILES) -T link.ld -o build/uOSkernel8.elf
	"aarch64-none-elf-objcopy" -O binary build/uOSkernel8.elf build/uOSkernel8.img

clean:
	rm -f *.o build/uOSkernel8.elf build/uOSkernel8.img
else ifeq ($(UNAME_S),Darwin)
    GCCPATH = "not set"
else ifeq ($(OS),Windows_NT)
GCCPATH = "D:/WindowsPrograms/aarc64GCC/14.2/bin"
all: clean uOSkernel8.img

%.o: %.c
	"$(GCCPATH)/aarch64-none-elf-gcc.exe" $(GCCFLAGS) -c $< -o $@

boot.o: boot.S
	"$(GCCPATH)/aarch64-none-elf-gcc.exe" $(GCCFLAGS) -c boot.S -o boot.o

uOSkernel8.img: boot.o $(OFILES)
	"$(GCCPATH)/aarch64-none-elf-ld.exe" -nostdlib -Wl,-Map=build/kernel.map boot.o $(OFILES) -T link.ld -o build/uOSkernel8.elf
	"$(GCCPATH)/aarch64-none-elf-objcopy.exe" -O binary build/uOSkernel8.elf build/uOSkernel8.img

clean:
	rm uOSkernel8.elf *.o *.img > /dev/null 2> /dev/null || true
else
    $(error "Unsupported platform: $(UNAME_S)")
endif