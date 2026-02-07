/* ZurichOS Kernel Entry Point
 * Main kernel initialization and startup
 */

#include <kernel/kernel.h>
#include <kernel/multiboot.h>
#include <drivers/vga.h>
#include <drivers/serial.h>
#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>
#include <drivers/keyboard.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <drivers/pit.h>
#include <drivers/pci.h>
#include <kernel/shell.h>
#include <acpi/acpi.h>
#include <apic/lapic.h>
#include <apic/ioapic.h>
#include <fs/vfs.h>
#include <fs/ramfs.h>
#include <fs/fat32.h>
#include <fs/devfs.h>
#include <fs/procfs.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/elf.h>
#include <drivers/ata.h>
#include <drivers/driver.h>
#include <drivers/isolation.h>
#include <syscall/syscall.h>
#include <kernel/symbols.h>
#include <security/security.h>
#include <drivers/framebuffer.h>
#include <drivers/mouse.h>
#include <string.h>

#define KERNEL_HEAP_START   0xD0000000
#define KERNEL_HEAP_SIZE    0x00800000  /* 8MB */

static void boot_delay(void)
{
    for (volatile int i = 0; i < 2000000; i++);
}

void kernel_main(multiboot_info_t *mboot_info, uint32_t magic)
{

    vga_init();
    vga_clear();

    serial_init();

    vga_puts("ZurichOS v0.1.0\n");
    vga_puts("================\n\n");

    serial_puts("[KERNEL] ZurichOS starting...\n");

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        vga_puts("ERROR: Invalid multiboot magic!\n");
        serial_puts("[KERNEL] ERROR: Invalid multiboot magic\n");
        goto halt;
    }

    vga_puts("Checking multiboot... ");
    serial_puts("[KERNEL] Multiboot verified\n");
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    vga_puts("Initializing GDT... ");
    serial_puts("[KERNEL] Initializing GDT\n");
    gdt_init();
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    vga_puts("Initializing IDT... ");
    serial_puts("[KERNEL] Initializing IDT\n");
    idt_init();
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    vga_puts("Initializing PMM... ");
    serial_puts("[KERNEL] Initializing PMM\n");
    multiboot_info_t *mboot_virt = (multiboot_info_t *)((uint32_t)mboot_info + KERNEL_VMA);
    pmm_init(mboot_virt);
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();
    
    serial_puts("[KERNEL] Total memory: ");
    uint32_t total_mb = pmm_get_total_memory() / (1024 * 1024);
    uint32_t free_mb = pmm_get_free_memory() / (1024 * 1024);
    vga_puts("  Memory: ");
    vga_put_dec(total_mb);
    vga_puts(" MB total, ");
    vga_put_dec(free_mb);
    vga_puts(" MB free\n");
    boot_delay();

    vga_puts("Initializing VMM... ");
    serial_puts("[KERNEL] Initializing VMM\n");
    vmm_init();
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    vga_puts("Initializing heap... ");
    serial_puts("[KERNEL] Initializing heap\n");
    heap_init(KERNEL_HEAP_START, KERNEL_HEAP_SIZE);
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    vga_puts("Initializing PIT... ");
    serial_puts("[KERNEL] Initializing PIT\n");
    pit_init(100);
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    vga_puts("Initializing keyboard... ");
    serial_puts("[KERNEL] Initializing keyboard\n");
    keyboard_init();
    keyboard_set_callback(vga_putchar);
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    vga_puts("Enabling interrupts... ");
    serial_puts("[KERNEL] Enabling interrupts\n");
    sti();
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    vga_puts("Testing heap... ");
    serial_puts("[KERNEL] Testing heap\n");
    char *test = kmalloc(64);
    if (test) {
        kfree(test);
        vga_puts_ok();
    } else {
        vga_puts_fail();
    }
    vga_puts("\n");
    boot_delay();

    vga_puts("Initializing ACPI... ");
    serial_puts("[KERNEL] Initializing ACPI\n");
    int acpi_result = acpi_init();
    if (acpi_result == 0) {
        vga_puts_ok();
        vga_puts("\n");
        boot_delay();
        
        uint32_t lapic_addr = acpi_get_lapic_addr();
        uint32_t ioapic_addr = acpi_get_ioapic_addr();
        
        if (lapic_addr && ioapic_addr) {
            vga_puts("Initializing APIC... ");
            serial_puts("[KERNEL] Initializing APIC\n");
            
            pic_disable();
            lapic_init(lapic_addr);
            ioapic_init(ioapic_addr);
            
            uint8_t lapic_id = lapic_get_id();
            
            /* Route IRQs through I/O APIC */
            /* IRQ0 (PIT timer) -> Vector 32 - still needed for calibration */
            ioapic_enable_irq(0, 32, lapic_id);
            /* IRQ1 (Keyboard) -> Vector 33 */
            ioapic_enable_irq(1, 33, lapic_id);
            /* IRQ12 (PS/2 Mouse) -> Vector 44 */
            ioapic_enable_irq(12, 44, lapic_id);
            /* IRQ14 (Primary ATA) -> Vector 46 */
            ioapic_enable_irq(14, 46, lapic_id);
            /* IRQ15 (Secondary ATA) -> Vector 47 */
            ioapic_enable_irq(15, 47, lapic_id);
            
            idt_set_apic_mode(1);
            
            lapic_timer_init(100);
            serial_puts("[KERNEL] LAPIC timer initialized at 100 Hz\n");
            
            pit_use_lapic_timer();
            ioapic_disable_irq(0);
            
            vga_puts_ok();
            vga_puts(" (LAPIC ID: ");
            vga_put_dec(lapic_id);
            vga_puts(", LAPIC timer active)\n");
            serial_puts("[KERNEL] APIC enabled, using LAPIC timer\n");
        } else {
            vga_puts("  APIC addresses not found, using PIC\n");
            serial_puts("[KERNEL] APIC not available, using PIC\n");
        }
    } else {
        vga_puts("not available (using PIC)\n");
        serial_puts("[KERNEL] ACPI not available\n");
    }
    boot_delay();

    vga_puts("Scanning PCI bus... ");
    serial_puts("[KERNEL] Scanning PCI bus\n");
    pci_init();
    vga_put_dec(pci_get_device_count());
    vga_puts(" devices found\n");
    boot_delay();

    vga_puts("Driver isolation... ");
    driver_isolation_init();
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    vga_puts("Registering drivers... ");
    serial_puts("[KERNEL] Registering PCI drivers\n");
    ata_pci_register();
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    vga_puts("Initializing framebuffer... ");
    serial_puts("[KERNEL] Checking for framebuffer\n");
    {
        int fb_ok = -1;
        if (mboot_virt->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) {
            uint32_t fb_addr = (uint32_t)mboot_virt->framebuffer_addr;
            uint32_t fb_width = mboot_virt->framebuffer_width;
            uint32_t fb_height = mboot_virt->framebuffer_height;
            uint32_t fb_pitch = mboot_virt->framebuffer_pitch;
            uint8_t fb_bpp = mboot_virt->framebuffer_bpp;
            uint8_t fb_type = mboot_virt->framebuffer_type;

            serial_printf("[KERNEL] FB: %dx%d %dbpp type=%d addr=0x%x pitch=%d\n",
                          fb_width, fb_height, fb_bpp, fb_type, fb_addr, fb_pitch);

            if (fb_type == 1) {
                /* Type 1 = EGA text mode, try BGA instead */
                serial_puts("[KERNEL] Multiboot reports text mode, trying BGA\n");
                fb_ok = fb_init_bga(1024, 768);
            } else {
                fb_ok = fb_init(fb_addr, fb_width, fb_height, fb_pitch, fb_bpp);
            }
        } else {
            serial_puts("[KERNEL] No multiboot FB info, trying BGA\n");
            fb_ok = fb_init_bga(1024, 768);
        }

        if (fb_ok == 0) {
            framebuffer_t *fbi = fb_get_info();
            vga_puts_ok();
            vga_puts(" (");
            vga_put_dec(fbi->width);
            vga_puts("x");
            vga_put_dec(fbi->height);
            vga_puts("x");
            vga_put_dec(fbi->bpp);
            vga_puts(")\n");
        } else {
            vga_puts("not available\n");
        }
    }
    boot_delay();

    vga_puts("Initializing mouse... ");
    serial_puts("[KERNEL] Initializing PS/2 mouse\n");
    mouse_init();
    if (fb_is_available()) {
        mouse_show_cursor();
    }
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    vga_puts("Initializing filesystem... ");
    serial_puts("[KERNEL] Initializing filesystem\n");
    vfs_init();
    vfs_node_t *ramfs_root = ramfs_init();
    if (ramfs_root) {
        vfs_set_root(ramfs_root);
        vga_puts_ok();
        vga_puts("\n");
        boot_delay();
        
        ramfs_create_dir(ramfs_root, "tmp");
        vfs_node_t *run_dir = ramfs_create_dir(ramfs_root, "run");
        if (run_dir) {
            ramfs_create_dir(run_dir, "lock");
            ramfs_create_dir(run_dir, "shm");
        }
        
        vfs_node_t *mnt_dir = ramfs_create_dir(ramfs_root, "mnt");
        if (mnt_dir) {
            ramfs_create_dir(mnt_dir, "cdrom");
            ramfs_create_dir(mnt_dir, "usb");
            ramfs_create_dir(mnt_dir, "floppy");
        }
        ramfs_create_dir(ramfs_root, "media");
        vfs_node_t *disks_dir = ramfs_create_dir(ramfs_root, "disks");
        if (disks_dir) {
            ramfs_create_dir(disks_dir, "hda");
            ramfs_create_dir(disks_dir, "hdb");
        }
        
        devfs_init(ramfs_root);
        procfs_init(ramfs_root);
        
        vfs_node_t *sys_dir = ramfs_create_dir(ramfs_root, "sys");
        if (sys_dir) {
            vfs_node_t *class_dir = ramfs_create_dir(sys_dir, "class");
            if (class_dir) {
                ramfs_create_dir(class_dir, "net");
                ramfs_create_dir(class_dir, "block");
            }
            ramfs_create_dir(sys_dir, "devices");
            ramfs_create_dir(sys_dir, "kernel");
        }
    } else {
        vga_puts_fail();
        vga_puts("\n");
    }

    vga_puts("Initializing processes... ");
    serial_puts("[KERNEL] Initializing processes\n");
    process_init();
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    vga_puts("Initializing scheduler... ");
    serial_puts("[KERNEL] Initializing scheduler\n");
    scheduler_init();
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    vga_puts("Scanning ATA drives... ");
    serial_puts("[KERNEL] Scanning ATA drives\n");
    ata_init();
    vga_put_dec(ata_get_drive_count());
    vga_puts(" drive(s) found\n");
    boot_delay();

    vga_puts("Mounting FAT32 disks... ");
    serial_puts("[KERNEL] Auto-mounting FAT32 disks\n");
    fat32_automount_all();
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    vga_puts("Mounting persistent dirs... ");
    serial_puts("[KERNEL] Mounting persistent directories from disk\n");
    {
        vfs_node_t *disk_root = vfs_lookup("/disks/hda");
        vfs_node_t *root = vfs_get_root();
        int mounted = 0;
        if (disk_root && root) {
            vfs_node_t *disk_node;
            
            /* /bin */
            disk_node = vfs_finddir(disk_root, "bin");
            if (disk_node) {
                ramfs_create_dir(root, "bin");
                if (vfs_mount("/bin", disk_node) == 0) mounted++;
            }
            /* /sbin */
            disk_node = vfs_finddir(disk_root, "sbin");
            if (disk_node) {
                ramfs_create_dir(root, "sbin");
                if (vfs_mount("/sbin", disk_node) == 0) mounted++;
            }
            /* /etc */
            disk_node = vfs_finddir(disk_root, "etc");
            if (disk_node) {
                ramfs_create_dir(root, "etc");
                if (vfs_mount("/etc", disk_node) == 0) mounted++;
            }
            /* /home */
            disk_node = vfs_finddir(disk_root, "home");
            if (disk_node) {
                ramfs_create_dir(root, "home");
                if (vfs_mount("/home", disk_node) == 0) mounted++;
            }
            /* /var */
            disk_node = vfs_finddir(disk_root, "var");
            if (disk_node) {
                ramfs_create_dir(root, "var");
                if (vfs_mount("/var", disk_node) == 0) mounted++;
            }
            /* /usr */
            disk_node = vfs_finddir(disk_root, "usr");
            if (disk_node) {
                ramfs_create_dir(root, "usr");
                if (vfs_mount("/usr", disk_node) == 0) mounted++;
            }
            /* /lib */
            disk_node = vfs_finddir(disk_root, "lib");
            if (disk_node) {
                ramfs_create_dir(root, "lib");
                if (vfs_mount("/lib", disk_node) == 0) mounted++;
            }
            /* /boot */
            disk_node = vfs_finddir(disk_root, "boot");
            if (disk_node) {
                ramfs_create_dir(root, "boot");
                if (vfs_mount("/boot", disk_node) == 0) mounted++;
            }
            /* /root */
            disk_node = vfs_lookup("/disks/hda/home/root");
            if (disk_node) {
                ramfs_create_dir(root, "root");
                if (vfs_mount("/root", disk_node) == 0) mounted++;
            }
            
            if (mounted > 0) {
                vga_puts_ok();
                vga_puts(" (");
                vga_put_dec(mounted);
                vga_puts(" dirs)");
            } else {
                vga_puts("0 dirs");
            }
            serial_puts("[KERNEL] Mounted persistent directories\n");
        } else {
            vga_puts("no disk");
            serial_puts("[KERNEL] No FAT32 disk found for persistent dirs\n");
        }
    }
    vga_puts("\n");
    boot_delay();

    vga_puts("Initializing syscalls... ");
    serial_puts("[KERNEL] Syscalls initialized\n");
    syscall_init();
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    vga_puts("Loading symbol table... ");
    serial_puts("[KERNEL] Symbol table initialized\n");
    symbols_init();
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    vga_puts("Initializing security... ");
    serial_puts("[KERNEL] Initializing security\n");
    security_init();
    vga_puts_ok();
    vga_puts("\n");
    boot_delay();

    serial_puts("[KERNEL] Security done, starting shell setup\n");
    vga_puts("\nKernel initialized successfully!\n");
    serial_puts("[KERNEL] Initialization complete\n");
    
    for (volatile int i = 0; i < 30000000; i++);
    
    serial_puts("[KERNEL] Clearing screen\n");
    vga_clear();
    
    serial_puts("[KERNEL] Allocating shell stack\n");
    uint32_t shell_stack_base = (uint32_t)kmalloc(16384);
    uint32_t shell_stack_top = shell_stack_base + 16384;
    serial_puts("[KERNEL] Setting shell stack\n");
    syscall_set_shell_stack(shell_stack_base, shell_stack_top);
    
    serial_puts("[KERNEL] Calling shell_init\n");
    shell_init();
    serial_puts("[KERNEL] Calling shell_run\n");
    shell_run();

halt:
    vga_puts("\nSystem halted.\n");
    serial_puts("[KERNEL] Halted\n");
    
    for (;;) {
        __asm__ volatile("hlt");
    }
}
