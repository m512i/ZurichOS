/* Shell Commands - System Category
 * time, date, timezone, lspci, apic, drivers, symbols
 */

#include <shell/builtins.h>
#include <kernel/shell.h>
#include <kernel/kernel.h>
#include <drivers/vga.h>
#include <drivers/rtc.h>
#include <drivers/pci.h>
#include <drivers/pit.h>
#include <drivers/driver.h>
#include <arch/x86/idt.h>
#include <apic/lapic.h>
#include <apic/ioapic.h>
#include <string.h>

extern uint32_t _kernel_start;
extern uint32_t _kernel_end;
extern uint32_t _kernel_end_phys;

void cmd_time(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    rtc_time_t time;
    rtc_get_local_time(&time);
    
    char buf[16];
    rtc_format_time(&time, buf);
    
    vga_puts("Time: ");
    vga_puts(buf);
    vga_puts(" (UTC");
    int8_t tz = rtc_get_timezone();
    if (tz >= 0) {
        vga_putchar('+');
    }
    vga_put_dec(tz < 0 ? -tz : tz);
    if (tz < 0) {
        vga_puts(" West");
    }
    vga_puts(")\n");
}

void cmd_date(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    rtc_time_t time;
    rtc_get_local_time(&time);
    
    char buf[16];
    rtc_format_date(&time, buf);
    
    const char *weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    
    vga_puts("Date: ");
    if (time.weekday >= 1 && time.weekday <= 7) {
        vga_puts(weekdays[time.weekday - 1]);
        vga_puts(" ");
    }
    vga_puts(buf);
    vga_puts("\n");
}

void cmd_timezone(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Current timezone: UTC");
        int8_t tz = rtc_get_timezone();
        if (tz >= 0) {
            vga_putchar('+');
        } else {
            vga_putchar('-');
            tz = -tz;
        }
        vga_put_dec(tz);
        vga_puts("\n");
        vga_puts("Usage: timezone <offset>\n");
        vga_puts("Examples: timezone -5  (EST)\n");
        vga_puts("          timezone -8  (PST)\n");
        vga_puts("          timezone 0   (UTC)\n");
        vga_puts("          timezone 1   (CET)\n");
        return;
    }
    
    const char *p = argv[1];
    int8_t sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    } else if (*p == '+') {
        p++;
    }
    
    int8_t offset = 0;
    while (*p >= '0' && *p <= '9') {
        offset = offset * 10 + (*p - '0');
        p++;
    }
    offset *= sign;
    
    rtc_set_timezone(offset);
    vga_puts("Timezone set to UTC");
    if (offset >= 0) {
        vga_putchar('+');
    }
    vga_put_dec(offset < 0 ? -offset : offset);
    if (offset < 0) {
        vga_puts(" (negative)");
    }
    vga_puts("\n");
}

static const char *pci_class_name(uint8_t class_code, uint8_t subclass)
{
    switch (class_code) {
        case 0x00: return "Unclassified";
        case 0x01:
            switch (subclass) {
                case 0x00: return "SCSI Controller";
                case 0x01: return "IDE Controller";
                case 0x05: return "ATA Controller";
                case 0x06: return "SATA Controller";
                case 0x08: return "NVMe Controller";
                default: return "Storage Controller";
            }
        case 0x02:
            switch (subclass) {
                case 0x00: return "Ethernet Controller";
                case 0x80: return "Network Controller";
                default: return "Network Controller";
            }
        case 0x03:
            switch (subclass) {
                case 0x00: return "VGA Controller";
                case 0x01: return "XGA Controller";
                case 0x02: return "3D Controller";
                default: return "Display Controller";
            }
        case 0x04: return "Multimedia Controller";
        case 0x05: return "Memory Controller";
        case 0x06:
            switch (subclass) {
                case 0x00: return "Host Bridge";
                case 0x01: return "ISA Bridge";
                case 0x04: return "PCI-to-PCI Bridge";
                case 0x80: return "Other Bridge";
                default: return "Bridge Device";
            }
        case 0x07: return "Communication Controller";
        case 0x08: return "System Peripheral";
        case 0x09: return "Input Device";
        case 0x0C:
            switch (subclass) {
                case 0x03: return "USB Controller";
                case 0x05: return "SMBus Controller";
                default: return "Serial Bus Controller";
            }
        case 0x0D: return "Wireless Controller";
        default: return "Unknown Device";
    }
}

void cmd_lspci(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    uint32_t count = pci_get_device_count();
    
    if (count == 0) {
        vga_puts("No PCI devices found.\n");
        return;
    }
    
    vga_puts("PCI Devices (");
    vga_put_dec(count);
    vga_puts(" found):\n\n");
    
    for (uint32_t i = 0; i < count; i++) {
        pci_device_t *dev = pci_get_device(i);
        if (!dev) continue;
        
        vga_put_hex(dev->bus);
        vga_puts(":");
        if (dev->device < 16) vga_putchar('0');
        vga_put_hex(dev->device);
        vga_puts(".");
        vga_put_dec(dev->function);
        vga_puts(" ");
        
        vga_put_hex(dev->vendor_id);
        vga_puts(":");
        vga_put_hex(dev->device_id);
        vga_puts(" ");
        
        vga_puts(pci_class_name(dev->class_code, dev->subclass));
        vga_puts("\n");
    }
}

void cmd_apic(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("APIC Status:\n");
    vga_puts("-----------\n");
    
    if (idt_is_apic_mode()) {
        vga_puts("Mode:        APIC (no legacy PIC)\n");
        vga_puts("LAPIC ID:    ");
        vga_put_dec(lapic_get_id());
        vga_puts("\n");
        vga_puts("LAPIC:       Enabled\n");
        
        if (lapic_is_enabled()) {
            vga_puts("Timer:       LAPIC @ ");
            vga_put_dec(lapic_get_frequency());
            vga_puts(" Hz\n");
            vga_puts("Ticks:       ");
            vga_put_dec(lapic_get_ticks());
            vga_puts("\n");
            vga_puts("Uptime:      ");
            vga_put_dec(lapic_get_uptime_sec());
            vga_puts(" seconds\n");
        }
        
        uint32_t max_irqs = ioapic_get_max_entries();
        vga_puts("I/O APIC:    Enabled (");
        vga_put_dec(max_irqs);
        vga_puts(" IRQ entries)\n");
        
        vga_puts("\nI/O APIC Redirection Table:\n");
        vga_puts("  IRQ  Vector  Mask  Dest\n");
        vga_puts("  ---  ------  ----  ----\n");
        
        for (uint32_t irq = 0; irq < max_irqs && irq < 24; irq++) {
            uint64_t entry = ioapic_get_entry(irq);
            uint8_t vector = entry & 0xFF;
            int masked = (entry >> 16) & 1;
            uint8_t dest = (entry >> 56) & 0xFF;
            
            if (vector != 0 || !masked) {
                vga_puts("  ");
                if (irq < 10) vga_puts(" ");
                vga_put_dec(irq);
                vga_puts("   ");
                if (vector < 100) vga_puts(" ");
                if (vector < 10) vga_puts(" ");
                vga_put_dec(vector);
                vga_puts("     ");
                vga_puts(masked ? "Y" : "N");
                vga_puts("     ");
                vga_put_dec(dest);
                vga_puts("\n");
            }
        }
        vga_puts("\n  LAPIC Timer -> Vector 32\n");
    } else {
        vga_puts("Mode:        Legacy PIC (8259)\n");
        vga_puts("Timer:       PIT @ ");
        vga_put_dec(pit_get_frequency());
        vga_puts(" Hz\n");
        vga_puts("Ticks:       ");
        vga_put_dec((uint32_t)pit_get_ticks());
        vga_puts("\n");
        vga_puts("Uptime:      ");
        vga_put_dec(pit_get_uptime_sec());
        vga_puts(" seconds\n");
        vga_puts("\nNote: APIC not available or not initialized\n");
    }
}

void cmd_drivers(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    uint32_t count = pci_get_driver_count();
    
    vga_puts("Registered PCI Drivers:\n");
    vga_puts("-----------------------\n");
    
    if (count == 0) {
        vga_puts("(no drivers registered)\n");
        return;
    }
    
    vga_puts("NAME             STATUS      DEVICES\n");
    
    for (uint32_t i = 0; i < count; i++) {
        pci_driver_t *drv = pci_get_driver_by_index(i);
        if (!drv) continue;
        
        vga_puts(drv->name);
        int len = strlen(drv->name);
        for (int j = len; j < 17; j++) vga_putchar(' ');
        
        switch (drv->status) {
            case DRIVER_STATUS_UNLOADED:
                vga_puts("unloaded    ");
                break;
            case DRIVER_STATUS_LOADED:
                vga_puts("loaded      ");
                break;
            case DRIVER_STATUS_ACTIVE:
                vga_puts("active      ");
                break;
            case DRIVER_STATUS_ERROR:
                vga_puts("error       ");
                break;
        }
        
        vga_put_dec(drv->devices_bound);
        vga_puts("\n");
    }
    
    vga_puts("\nDevice-Driver Bindings:\n");
    uint32_t bindings = pci_get_binding_count();
    
    if (bindings == 0) {
        vga_puts("(no devices bound)\n");
    } else {
        for (uint32_t i = 0; i < bindings; i++) {
            pci_binding_t *bind = pci_get_binding(i);
            if (!bind) continue;
            
            vga_puts("  ");
            vga_put_hex(bind->device->bus);
            vga_puts(":");
            vga_put_hex(bind->device->device);
            vga_puts(".");
            vga_put_dec(bind->device->function);
            vga_puts(" -> ");
            vga_puts(bind->driver->name);
            vga_puts("\n");
        }
    }
}

void cmd_symbols(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("Kernel Addresses:\n");
    vga_puts("  _kernel_start:    ");
    vga_put_hex((uint32_t)&_kernel_start);
    vga_puts("\n");
    vga_puts("  _kernel_end:      ");
    vga_put_hex((uint32_t)&_kernel_end);
    vga_puts("\n");
    vga_puts("  _kernel_end_phys: ");
    vga_put_hex((uint32_t)&_kernel_end_phys);
    vga_puts("\n");
    
    vga_puts("\nHardware MMIO (virtual):\n");
    vga_puts("  VGA buffer:       ");
    vga_put_hex(vga_get_buffer_addr());
    vga_puts("\n");
    vga_puts("  LAPIC:            ");
    vga_put_hex(LAPIC_BASE_VIRT);
    vga_puts("\n");
    vga_puts("  I/O APIC:         ");
    vga_put_hex(IOAPIC_BASE_VIRT);
    vga_puts("\n");
    
    vga_puts("\nUseful for hexdump:\n");
    vga_puts("  hexdump ");
    vga_put_hex(vga_get_buffer_addr());
    vga_puts(" 80   (VGA buffer)\n");
}
