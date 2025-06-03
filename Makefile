CFILES = $(wildcard *.c)
OFILES = $(CFILES:.c=.o)
GCCFLAGS = -Wall -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles -Wno-unused-variable -Wno-int-conversion -Wno-int-to-pointer-cast -Wno-array-bounds -Wno-unused-but-set-variable -Wno-stringop-overflow

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
GCCPATH = "/media/urdec-lx/common_disk/LinuxPrograms/aarch64GCC/bin"

all: clean uOSkernel8.img

%.o: %.c
	"$(GCCPATH)/aarch64-none-elf-gcc" $(GCCFLAGS) -c $< -o $@

boot.o: boot.S
	"$(GCCPATH)/aarch64-none-elf-gcc" $(GCCFLAGS) -c boot.S -o boot.o

uOSkernel8.img: boot.o $(OFILES)
	"$(GCCPATH)/aarch64-none-elf-ld" -nostdlib boot.o $(OFILES) -T link.ld -o build/uOSkernel8.elf
	"$(GCCPATH)/aarch64-none-elf-objcopy" -O binary build/uOSkernel8.elf build/uOSkernel8.img

clean:
	/bin/rm uOSkernel8.elf *.o *.img > /dev/null 2> /dev/null || true
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
	"$(GCCPATH)/aarch64-none-elf-ld.exe" -nostdlib boot.o $(OFILES) -T link.ld -o build/uOSkernel8.elf
	"$(GCCPATH)/aarch64-none-elf-objcopy.exe" -O binary build/uOSkernel8.elf build/uOSkernel8.img

clean:
	/bin/rm uOSkernel8.elf *.o *.img > /dev/null 2> /dev/null || true
else
    $(error "Unsupported platform: $(UNAME_S)")
endif