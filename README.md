# uOS

uOS is an experimental AArch64 kernel targeting the Raspberry Pi 4 (Cortex-A72). It is written in C and AArch64 assembly and currently initializes low-level hardware, memory, threading, supervisor requests, virtual filesystems, and eMMC/SD storage.

## Requirements

- Raspberry Pi 4 with a 64-bit firmware configuration
- AArch64 bare-metal GNU toolchain (`aarch64-none-elf-gcc`, `aarch64-none-elf-ld`, and `aarch64-none-elf-objcopy`)
- GNU Make
- UART connection configured for 115200 baud

The build file currently assumes the cross-compiler is available by name on Linux. Windows uses the path configured in `Makefile`; macOS is not configured yet.

## Build

```sh
make
```

The build produces `build/uOSkernel8.img`, `build/uOSkernel8.elf`, and `build/kernel.map`.

To remove generated object files and images:

```sh
make clean
```

## Run on hardware

Copy `build/uOSkernel8.img` to the boot media using the boot setup required by your Raspberry Pi firmware, then connect to the kernel UART at 115200 baud. The kernel currently performs SD/eMMC initialization during startup and prints its progress over UART.

## Repository layout

```text
boot.S                         AArch64 entry point and exception vectors
kernel.c                       Kernel startup sequence
link.ld                        Raspberry Pi 4 memory layout and linker script
u_kernel/                      Kernel subsystems and drivers
UOS_Supervisor_call_ids.txt    Supervisor request identifiers
Makefile                       Cross-compilation and image generation
```

## Status

This is an active experimental kernel project. Interfaces and hardware support may change as the kernel develops.

## License

No license file is currently included in the repository.
