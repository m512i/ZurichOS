/* Shell Commands - Driver Isolation
 * Inspect and manage driver isolation domains
 */

#include <shell/builtins.h>
#include <drivers/isolation.h>
#include <drivers/driver.h>
#include <drivers/vga.h>
#include <string.h>

void cmd_isolation(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "domains") == 0) {
        uint32_t count = driver_domain_count();
        if (count == 0) {
            vga_puts("No active driver domains.\n");
            return;
        }
        
        vga_puts("Driver Isolation Domains:\n");
        vga_puts("ID  Name             Ring  KCalls  IOViol  IOOps\n");
        vga_puts("--  ----             ----  ------  ------  -----\n");
        
        for (uint32_t i = 0; i < count; i++) {
            driver_domain_t *d = driver_domain_get_by_index(i);
            if (!d) continue;
            
            vga_put_dec(d->id);
            if (d->id < 10) vga_puts(" ");
            vga_puts("  ");
            
            vga_puts(d->name);
            int namelen = strlen(d->name);
            for (int j = namelen; j < 17; j++) vga_puts(" ");
            
            vga_puts(d->isolation_level == 1 ? "R1    " : "R0    ");
            
            vga_put_dec(d->kernel_calls);
            if (d->kernel_calls < 10) vga_puts("     ");
            else if (d->kernel_calls < 100) vga_puts("    ");
            else if (d->kernel_calls < 1000) vga_puts("   ");
            else vga_puts("  ");
            
            vga_put_dec(d->io_violations);
            if (d->io_violations < 10) vga_puts("     ");
            else if (d->io_violations < 100) vga_puts("    ");
            else vga_puts("   ");
            
            vga_put_dec(d->total_io_ops);
            vga_puts("\n");
        }
        return;
    }
    
    if (argc >= 2 && strcmp(argv[1], "drivers") == 0) {
        uint32_t count = pci_get_driver_count();
        if (count == 0) {
            vga_puts("No registered drivers.\n");
            return;
        }
        
        vga_puts("Driver Isolation Status:\n");
        vga_puts("Name             Status   Isolation  Ports\n");
        vga_puts("----             ------   ---------  -----\n");
        
        for (uint32_t i = 0; i < count; i++) {
            pci_driver_t *drv = pci_get_driver_by_index(i);
            if (!drv) continue;
            
            vga_puts(drv->name);
            int namelen = strlen(drv->name);
            for (int j = namelen; j < 17; j++) vga_puts(" ");
            
            switch (drv->status) {
                case DRIVER_STATUS_ACTIVE:   vga_puts("Active   "); break;
                case DRIVER_STATUS_LOADED:   vga_puts("Loaded   "); break;
                case DRIVER_STATUS_ERROR:    vga_puts("Error    "); break;
                default:                     vga_puts("Unknown  "); break;
            }
            
            if (drv->domain) {
                vga_puts("Ring 1     ");
            } else {
                vga_puts("Ring 0     ");
            }
            
            if (drv->io_port_count > 0) {
                vga_put_hex(drv->io_port_base);
                vga_puts("+");
                vga_put_dec(drv->io_port_count);
            } else {
                vga_puts("all");
            }
            vga_puts("\n");
        }
        return;
    }
    
    vga_puts("Driver Isolation Framework\n");
    vga_puts("=========================\n");
    vga_puts("Ring 0: Kernel (full access)\n");
    vga_puts("Ring 1: Drivers (I/O restricted via IOPB)\n");
    vga_puts("Ring 2: Services (reserved)\n");
    vga_puts("Ring 3: User programs\n\n");
    
    uint32_t total_drivers = pci_get_driver_count();
    uint32_t isolated = 0;
    for (uint32_t i = 0; i < total_drivers; i++) {
        pci_driver_t *drv = pci_get_driver_by_index(i);
        if (drv && drv->domain) isolated++;
    }
    
    vga_puts("Registered drivers: ");
    vga_put_dec(total_drivers);
    vga_puts("\n");
    vga_puts("Isolated (Ring 1):  ");
    vga_put_dec(isolated);
    vga_puts("\n");
    vga_puts("Unrestricted (R0):  ");
    vga_put_dec(total_drivers - isolated);
    vga_puts("\n");
    vga_puts("Active domains:     ");
    vga_put_dec(driver_domain_count());
    vga_puts("\n\n");
    vga_puts("Commands:\n");
    vga_puts("  isolation domains  - Show isolation domains\n");
    vga_puts("  isolation drivers  - Show driver isolation status\n");
}
