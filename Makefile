
ifeq ($(OS),Windows_NT)
    DETECTED_OS := Windows
    MKDIR := mkdir -p
    RM := rm -rf
    QEMU := qemu-system-i386
else
    DETECTED_OS := $(shell uname -s)
    MKDIR := mkdir -p
    RM := rm -rf
    QEMU := qemu-system-i386
endif

ifeq ($(OS),Windows_NT)
    CROSS_PREFIX ?= D:/i686/bin/i686-elf-
else
    CROSS_PREFIX ?= i686-elf-
endif
CC := $(CROSS_PREFIX)gcc
AS := $(CROSS_PREFIX)as
LD := $(CROSS_PREFIX)ld
AR := $(CROSS_PREFIX)ar
OBJCOPY := $(CROSS_PREFIX)objcopy
OBJDUMP := $(CROSS_PREFIX)objdump

NASM := nasm

SRC_DIR := .
BUILD_DIR := build
ISO_DIR := iso
KERNEL_DIR := kernel
BOOT_DIR := boot
LIBC_DIR := libc
INCLUDE_DIR := include

KERNEL_BIN := $(BUILD_DIR)/zurichos.bin
KERNEL_ELF := $(BUILD_DIR)/zurichos.elf
ISO_FILE := $(BUILD_DIR)/zurichos.iso

CFLAGS := -ffreestanding \
          -nostdlib \
          -nostdinc \
          -fno-builtin \
          -fno-stack-protector \
          -fno-pic \
          -m32 \
          -march=i686 \
          -Wall \
          -Wextra \
          -Werror \
          -g \
          -I$(INCLUDE_DIR) \
          -I$(KERNEL_DIR)/include \
          -I$(LIBC_DIR)/include

NASMFLAGS := -f elf32 -g -F dwarf

LDFLAGS := -T config/linker.ld \
           -nostdlib \
           -ffreestanding \
           -m32

LIBS := -lgcc

BOOT_ASM_SRC := $(wildcard $(BOOT_DIR)/*.asm)
KERNEL_C_SRC := $(wildcard $(KERNEL_DIR)/*.c) \
                $(wildcard $(KERNEL_DIR)/arch/x86/*.c) \
                $(wildcard $(KERNEL_DIR)/drivers/*/*.c) \
                $(wildcard $(KERNEL_DIR)/mm/*.c) \
                $(wildcard $(KERNEL_DIR)/sync/*.c) \
                $(wildcard $(KERNEL_DIR)/acpi/*.c) \
                $(wildcard $(KERNEL_DIR)/apic/*.c) \
                $(wildcard $(KERNEL_DIR)/syscall/*.c) \
                $(wildcard $(KERNEL_DIR)/shell/*.c) \
                $(wildcard $(KERNEL_DIR)/shell/cmds/*.c) \
                $(wildcard $(KERNEL_DIR)/shell/shell/*.c) \
                $(wildcard $(KERNEL_DIR)/fs/*.c) \
                $(wildcard $(KERNEL_DIR)/sched/*.c) \
                $(wildcard $(KERNEL_DIR)/elf/*.c) \
                $(wildcard $(KERNEL_DIR)/signal/*.c) \
                $(wildcard $(KERNEL_DIR)/ipc/*.c) \
                $(wildcard $(KERNEL_DIR)/process/*.c) \
                $(wildcard $(KERNEL_DIR)/core/*.c) \
                $(wildcard $(KERNEL_DIR)/security/*.c) \
                $(wildcard drivers/*/*.c) \
                $(wildcard drivers/*/*/*.c) \
                $(wildcard services/*/*.c)
KERNEL_ASM_SRC := $(wildcard $(KERNEL_DIR)/arch/x86/*.asm) \
                  $(wildcard $(KERNEL_DIR)/sched/*.asm)
LIBC_C_SRC := $(wildcard $(LIBC_DIR)/string/*.c) \
              $(wildcard $(LIBC_DIR)/stdio/*.c) \
              $(wildcard $(LIBC_DIR)/stdlib/*.c)

BOOT_OBJ := $(patsubst $(BOOT_DIR)/%.asm,$(BUILD_DIR)/boot/%.o,$(BOOT_ASM_SRC))
KERNEL_C_OBJ := $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SRC))
KERNEL_ASM_OBJ := $(patsubst %.asm,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SRC))
LIBC_OBJ := $(patsubst %.c,$(BUILD_DIR)/%.o,$(LIBC_C_SRC))

ALL_OBJ := $(BOOT_OBJ) $(KERNEL_C_OBJ) $(KERNEL_ASM_OBJ) $(LIBC_OBJ)

# User-mode programs
USER_UTILS := hello counter fibonacci primes banner memtest filetest isolate ctortest ipctest vmtest nettest sectest seccrash
USER_SERVICES := 
USER_INIT := init
USER_SHELL := shell

USER_UTILS_ELF := $(patsubst %,usermode/test/%.elf,$(USER_UTILS))
USER_SERVICES_ELF := $(patsubst %,usermode/services/%.elf,$(USER_SERVICES))
USER_INIT_ELF := $(patsubst %,usermode/init/%.elf,$(USER_INIT))
USER_SHELL_ELF := $(patsubst %,usermode/shell/%.elf,$(USER_SHELL))

USER_ELFS := $(USER_UTILS_ELF) $(USER_SERVICES_ELF) $(USER_INIT_ELF) $(USER_SHELL_ELF)

.PHONY: all clean run debug iso dirs toolchain-check user-progs copy-progs

all: dirs toolchain-check $(KERNEL_ELF) user-progs
	@echo "Build complete: $(KERNEL_ELF)"

user-progs: $(USER_ELFS) copy-progs

# Build rules for different user-mode components
usermode/test/%.elf: usermode/test/%.c
	@echo "CC $<"
	@$(CC) -m32 -ffreestanding -nostdlib -fno-stack-protector -o $@ $< -T usermode/link.ld

usermode/services/%.elf: usermode/services/%.c
	@echo "CC $<"
	@$(CC) -m32 -ffreestanding -nostdlib -fno-stack-protector -o $@ $< -T usermode/link.ld

usermode/init/%.elf: usermode/init/%.c
	@echo "CC $<"
	@$(CC) -m32 -ffreestanding -nostdlib -fno-stack-protector -o $@ $< -T usermode/link.ld

usermode/shell/%.elf: usermode/shell/%.c
	@echo "CC $<"
	@$(CC) -m32 -ffreestanding -nostdlib -fno-stack-protector -o $@ $< -T usermode/link.ld

usermode/test/%.elf: usermode/test/%.cpp
	@echo "CXX $<"
	@$(TOOLCHAIN_PREFIX)g++ -m32 -ffreestanding -nostdlib -fno-stack-protector -fno-exceptions -fno-rtti -o $@ $< -T usermode/link.ld

copy-progs: $(USER_ELFS)
	@echo "Setting up disk filesystem..."
	@if [ -f disks/fat32disk.img ]; then \
		mmd -D s -i disks/fat32disk.img ::bin ::sbin ::etc ::home ::var ::usr ::lib ::boot 2>/dev/null || true; \
		mmd -D s -i disks/fat32disk.img ::etc/rc.d ::home/root ::home/user 2>/dev/null || true; \
		mmd -D s -i disks/fat32disk.img ::home/user/documents ::home/user/downloads 2>/dev/null || true; \
		mmd -D s -i disks/fat32disk.img ::var/log ::var/run ::var/tmp ::var/spool ::var/cache ::var/lib 2>/dev/null || true; \
		mmd -D s -i disks/fat32disk.img ::usr/bin ::usr/sbin ::usr/lib ::usr/share 2>/dev/null || true; \
		mmd -D s -i disks/fat32disk.img ::usr/share/doc ::usr/share/man ::lib/modules 2>/dev/null || true; \
		echo "root:x:0:0:root:/root:/bin/sh" > /tmp/zos_passwd; \
		echo "user:x:1000:1000:User:/home/user:/bin/sh" >> /tmp/zos_passwd; \
		mcopy -D o -i disks/fat32disk.img /tmp/zos_passwd ::etc/passwd; \
		echo "root:x:0:" > /tmp/zos_group; \
		echo "users:x:100:user" >> /tmp/zos_group; \
		mcopy -D o -i disks/fat32disk.img /tmp/zos_group ::etc/group; \
		echo "zurich" > /tmp/zos_hostname; \
		mcopy -D o -i disks/fat32disk.img /tmp/zos_hostname ::etc/hostname; \
		echo "127.0.0.1 localhost" > /tmp/zos_hosts; \
		mcopy -D o -i disks/fat32disk.img /tmp/zos_hosts ::etc/hosts; \
		echo "export PATH=/bin:/sbin:/usr/bin" > /tmp/zos_profile; \
		echo "export HOME=/root" >> /tmp/zos_profile; \
		echo "export USER=root" >> /tmp/zos_profile; \
		echo "export SHELL=/bin/sh" >> /tmp/zos_profile; \
		echo "export HOSTNAME=zurich" >> /tmp/zos_profile; \
		echo "export TERM=vga" >> /tmp/zos_profile; \
		mcopy -D o -i disks/fat32disk.img /tmp/zos_profile ::etc/profile; \
		mcopy -D o -i disks/fat32disk.img /tmp/zos_profile ::home/root/.profile 2>/dev/null || true; \
		rm -f /tmp/zos_*; \
		for elf in $(USER_UTILS_ELF); do \
			filename=$$(basename $$elf .elf); \
			mcopy -D o -i disks/fat32disk.img $$elf ::usr/bin/$$filename; \
		done; \
		for elf in $(USER_INIT_ELF); do \
			filename=$$(basename $$elf .elf); \
			mcopy -D o -i disks/fat32disk.img $$elf ::sbin/$$filename; \
		done; \
		for elf in $(USER_SHELL_ELF); do \
			filename=$$(basename $$elf .elf); \
			mcopy -D o -i disks/fat32disk.img $$elf ::bin/$$filename; \
		done; \
		echo "Disk setup complete"; \
	else \
		echo "Warning: No disk image found"; \
	fi

dirs:
	@mkdir -p $(BUILD_DIR)/boot
	@mkdir -p $(BUILD_DIR)/kernel/arch/x86
	@mkdir -p $(BUILD_DIR)/kernel/drivers/serial
	@mkdir -p $(BUILD_DIR)/kernel/drivers/keyboard
	@mkdir -p $(BUILD_DIR)/kernel/drivers/vga
	@mkdir -p $(BUILD_DIR)/kernel/drivers/mouse
	@mkdir -p $(BUILD_DIR)/kernel/drivers/pci
	@mkdir -p $(BUILD_DIR)/kernel/drivers/pit
	@mkdir -p $(BUILD_DIR)/kernel/drivers/rtc
	@mkdir -p $(BUILD_DIR)/kernel/drivers/speaker
	@mkdir -p $(BUILD_DIR)/kernel/drivers/ata
	@mkdir -p $(BUILD_DIR)/kernel/drivers/framebuffer
	@mkdir -p $(BUILD_DIR)/kernel/drivers/isolation
	@mkdir -p $(BUILD_DIR)/kernel/mm
	@mkdir -p $(BUILD_DIR)/kernel/sync
	@mkdir -p $(BUILD_DIR)/kernel/acpi
	@mkdir -p $(BUILD_DIR)/kernel/apic
	@mkdir -p $(BUILD_DIR)/kernel/syscall
	@mkdir -p $(BUILD_DIR)/kernel/shell
	@mkdir -p $(BUILD_DIR)/kernel/shell/cmds
	@mkdir -p $(BUILD_DIR)/kernel/shell/shell
	@mkdir -p $(BUILD_DIR)/kernel/fs
	@mkdir -p $(BUILD_DIR)/kernel/sched
	@mkdir -p $(BUILD_DIR)/kernel/elf
	@mkdir -p $(BUILD_DIR)/kernel/signal
	@mkdir -p $(BUILD_DIR)/kernel/ipc
	@mkdir -p $(BUILD_DIR)/kernel/process
	@mkdir -p $(BUILD_DIR)/kernel/core
	@mkdir -p $(BUILD_DIR)/kernel/security
	@mkdir -p $(BUILD_DIR)/drivers/net
	@mkdir -p $(BUILD_DIR)/drivers/block
	@mkdir -p $(BUILD_DIR)/drivers/char
	@mkdir -p $(BUILD_DIR)/drivers/bus
	@mkdir -p $(BUILD_DIR)/drivers/timer
	@mkdir -p $(BUILD_DIR)/services/net
	@mkdir -p $(BUILD_DIR)/services/fs
	@mkdir -p $(BUILD_DIR)/libc/string
	@mkdir -p $(BUILD_DIR)/libc/stdio
	@mkdir -p $(BUILD_DIR)/libc/stdlib
	@mkdir -p $(ISO_DIR)/boot/grub

toolchain-check:
	@which $(CC) > /dev/null 2>&1 || \
		(echo "Error: Cross-compiler $(CC) not found!" && \
		 echo "Please build the cross-compiler first." && \
		 echo "See scripts/toolchain/build-toolchain.sh" && \
		 exit 1)

$(KERNEL_ELF): $(ALL_OBJ)
	@echo "LD $@"
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BUILD_DIR)/boot/%.o: $(BOOT_DIR)/%.asm
	@echo "NASM $<"
	@$(NASM) $(NASMFLAGS) -o $@ $<

$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/%.c
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/%.asm
	@echo "NASM $<"
	@$(NASM) $(NASMFLAGS) -o $@ $<

$(BUILD_DIR)/libc/%.o: $(LIBC_DIR)/%.c
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/drivers/%.o: drivers/%.c
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/services/%.o: services/%.c
	@echo "CC $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

iso: all
	@echo "Creating ISO..."
	@cp $(KERNEL_ELF) $(ISO_DIR)/boot/zurichos.elf
	@cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/
	@grub-mkrescue -o $(ISO_FILE) $(ISO_DIR) 2>/dev/null
	@echo "ISO created: $(ISO_FILE)"

DISK_IMAGES := $(wildcard disks/*.img)
DISK_ARGS := $(foreach img,$(DISK_IMAGES),-drive file=$(img),format=raw,if=ide)

run: all
	@echo "Attaching disks: $(DISK_IMAGES)"
	qemu-system-i386 -kernel ./build/zurichos.elf \
		-serial stdio \
		-m 256M \
		-no-shutdown \
		-vga std \
		-audiodev dsound,id=audio0 \
		-machine pcspk-audiodev=audio0 \
		-netdev user,id=net0,hostfwd=tcp::5555-:80 \
		-device e1000,netdev=net0 \
		$(DISK_ARGS)

run-iso: iso
	@echo "Attaching disks: $(DISK_IMAGES)"
	qemu-system-i386 -cdrom $(ISO_FILE) \
		-serial stdio \
		-m 256M \
		-no-shutdown \
		-vga std \
		-audiodev dsound,id=audio0 \
		-machine pcspk-audiodev=audio0 \
		$(DISK_ARGS)

debug: all
	@echo "Starting QEMU with GDB stub on port 1234..."
	@echo "Connect with: i686-elf-gdb -x .gdbinit"
	@echo "Or manually: i686-elf-gdb $(KERNEL_ELF) -ex 'set arch i386' -ex 'target remote localhost:1234'"
	qemu-system-i386 -kernel $(KERNEL_ELF) \
		-serial stdio \
		-m 256M \
		-no-shutdown \
		-vga std \
		-cpu 486 \
		-audiodev dsound,id=audio0 \
		-machine pcspk-audiodev=audio0 \
		$(DISK_ARGS) \
		-s -S

disasm: all
	$(OBJDUMP) -d $(KERNEL_ELF) > $(BUILD_DIR)/zurichos.disasm
	@echo "Disassembly: $(BUILD_DIR)/zurichos.disasm"

sections: all
	$(OBJDUMP) -h $(KERNEL_ELF)

symbols: all
	$(OBJDUMP) -t $(KERNEL_ELF) | sort

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(ISO_DIR)/boot/zurichos.elf
	rm -f usermode/test/*.elf
	rm -f usermode/init/*.elf
	rm -f usermode/shell/*.elf
	rm -f usermode/services/*.elf

help:
	@echo "ZurichOS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build kernel (default)"
	@echo "  iso      - Create bootable ISO"
	@echo "  run      - Run kernel in QEMU"
	@echo "  run-iso  - Run ISO in QEMU"
	@echo "  debug    - Run with GDB debugging"
	@echo "  disasm   - Generate disassembly"
	@echo "  sections - Show ELF sections"
	@echo "  symbols  - Show symbol table"
	@echo "  clean    - Remove build artifacts"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Disk Images:"
	@echo "  Place .img files in disks/ folder"
	@echo "  They will be auto-attached as hda, hdb, etc."
