/* Driver Isolation - Ring 1 Execution Framework
 * Provides I/O port permission control, Ring 1 stack management,
 * and kernel service call gate for isolated drivers.
 */

#include <drivers/isolation.h>
#include <drivers/serial.h>
#include <kernel/kernel.h>
#include <arch/x86/idt.h>
#include <mm/heap.h>
#include <string.h>

static driver_domain_t domains[MAX_DRIVER_DOMAINS];
static int next_domain_id = 1;
static driver_domain_t *current_domain = NULL;

static ring1_context_t ring1_ctx;

static int (*ring1_func)(void *);
static void *ring1_arg;

/* TSS + IOPB are now managed in gdt.c as a contiguous tss_block.
 * We use tss_set_iopb/tss_clear_iopb/tss_deny_all_iopb to update it. */

void driver_isolation_init(void)
{
    memset(domains, 0, sizeof(domains));
    next_domain_id = 1;
    current_domain = NULL;

    tss_deny_all_iopb();

    serial_puts("[ISOLATION] Driver isolation subsystem initialized\n");
    serial_puts("[ISOLATION] Ring 1 segments: CS=0x");
    char buf[16];
    uint32_t cs = GDT_DRIVER_CODE_SEGMENT | RING_DRIVER;
    uint32_t ds = GDT_DRIVER_DATA_SEGMENT | RING_DRIVER;
    int i;

    i = 0;
    do { buf[i++] = "0123456789ABCDEF"[(cs >> 4) & 0xF]; buf[i++] = "0123456789ABCDEF"[cs & 0xF]; } while (0);
    buf[i] = '\0';
    serial_puts(buf);
    serial_puts(" DS=0x");
    i = 0;
    do { buf[i++] = "0123456789ABCDEF"[(ds >> 4) & 0xF]; buf[i++] = "0123456789ABCDEF"[ds & 0xF]; } while (0);
    buf[i] = '\0';
    serial_puts(buf);
    serial_puts("\n");

    driver_isolation_install_handlers();
}

driver_domain_t *driver_domain_create(const char *name, int isolation_level)
{
    if (!name) return NULL;

    for (int i = 0; i < MAX_DRIVER_DOMAINS; i++) {
        if (!domains[i].active) {
            driver_domain_t *d = &domains[i];
            memset(d, 0, sizeof(driver_domain_t));

            d->id = next_domain_id++;
            d->name = name;
            d->isolation_level = isolation_level;
            d->active = 1;
            d->kernel_calls = 0;
            d->io_violations = 0;
            d->total_io_ops = 0;

            memset(d->iopb, 0xFF, IOPB_SIZE);

            if (isolation_level == DRIVER_ISOLATION_RING1) {
                void *stack = kmalloc(DRIVER_STACK_SIZE);
                if (stack) {
                    d->stack_base = (uint32_t)stack;
                    d->stack_top = d->stack_base + DRIVER_STACK_SIZE;
                } else {
                    serial_puts("[ISOLATION] WARNING: Failed to allocate Ring 1 stack for ");
                    serial_puts(name);
                    serial_puts("\n");
                    d->active = 0;
                    return NULL;
                }
            }

            serial_puts("[ISOLATION] Created domain '");
            serial_puts(name);
            serial_puts("' id=");
            char id_buf[8];
            int idx = 0;
            int n = d->id;
            if (n == 0) { id_buf[idx++] = '0'; }
            else {
                char tmp[8]; int ti = 0;
                while (n > 0) { tmp[ti++] = '0' + (n % 10); n /= 10; }
                while (ti > 0) id_buf[idx++] = tmp[--ti];
            }
            id_buf[idx] = '\0';
            serial_puts(id_buf);
            serial_puts(isolation_level == DRIVER_ISOLATION_RING1 ? " [Ring 1]\n" : " [Ring 0]\n");

            return d;
        }
    }

    serial_puts("[ISOLATION] ERROR: No free domain slots\n");
    return NULL;
}

void driver_domain_destroy(driver_domain_t *domain)
{
    if (!domain || !domain->active) return;

    if (current_domain == domain) {
        driver_domain_deactivate();
    }

    if (domain->stack_base) {
        kfree((void *)domain->stack_base);
    }

    serial_puts("[ISOLATION] Destroyed domain '");
    serial_puts(domain->name);
    serial_puts("'\n");

    domain->active = 0;
}

void driver_domain_allow_port(driver_domain_t *domain, uint16_t port_start, uint16_t port_count)
{
    if (!domain) return;

    for (uint16_t i = 0; i < port_count; i++) {
        uint16_t port = port_start + i;
        domain->iopb[port / 8] &= ~(1 << (port % 8));
    }
}

void driver_domain_deny_port(driver_domain_t *domain, uint16_t port_start, uint16_t port_count)
{
    if (!domain) return;

    for (uint16_t i = 0; i < port_count; i++) {
        uint16_t port = port_start + i;
        domain->iopb[port / 8] |= (1 << (port % 8));
    }
}

void driver_domain_allow_single_port(driver_domain_t *domain, uint16_t port)
{
    driver_domain_allow_port(domain, port, 1);
}

void driver_domain_activate(driver_domain_t *domain)
{
    if (!domain || !domain->active) return;

    current_domain = domain;

    if (domain->isolation_level == DRIVER_ISOLATION_RING1) {
        tss_set_ring1_stack(domain->stack_top, GDT_DRIVER_DATA_SEGMENT | RING_DRIVER);
        tss_set_iopb(domain->iopb, IOPB_SIZE);
    }
}

void driver_domain_deactivate(void)
{
    current_domain = NULL;
    tss_clear_iopb();
}

/*
 * Ring 1 trampoline - this function executes in Ring 1.
 * It calls the driver function, then uses INT 0x82 to return to Ring 0.
 * The function pointer and arg are passed via the static globals
 * ring1_func and ring1_arg (set before the IRET transition).
 *
 * Note: This code runs with Ring 1 CS/DS segments. The CPU enforces
 * IOPB restrictions because IOPL=0 and CPL=1 (CPL > IOPL).
 * Direct IN/OUT instructions will cause #GP if the IOPB bit is set.
 */
static void __attribute__((used)) ring1_trampoline(void)
{
    int ret = ring1_func(ring1_arg);

    /* Return to Ring 0 via INT 0x82, passing return value in EAX */
    __asm__ volatile(
        "mov %0, %%eax\n\t"
        "int $0x82\n\t"
        :
        : "r"(ret)
        : "eax"
    );

    for (;;) __asm__ volatile("hlt");
}

/*
 * INT 0x81 handler - Driver kernel service call
 * Called from Ring 1 via INT 0x81
 * Registers: EAX=service_id, EBX=arg1, ECX=arg2, EDX=arg3
 * Return value placed in saved EAX on the interrupt frame
 */
static void ring1_service_handler(registers_t *regs)
{
    uint32_t result = driver_kernel_service(regs->eax, regs->ebx, regs->ecx, regs->edx);
    regs->eax = result;
}

/*
 * INT 0x82 handler - Driver return
 * Called from Ring 1 trampoline when the driver function completes.
 * EAX contains the driver function's return value.
 *
 * We bypass the normal ISR return path entirely by restoring the
 * saved Ring 0 ESP/EBP and jumping directly to the resume point.
 * This avoids IRET same-privilege vs cross-privilege stack issues.
 *
 * We're already in Ring 0 (the INT handler runs in Ring 0), so we
 * just need to restore the kernel stack state and jump back.
 */
static void ring1_return_handler(registers_t *regs)
{
    if (!ring1_ctx.valid) {
        serial_puts("[ISOLATION] ERROR: INT 0x82 with no valid ring1 context!\n");
        return;
    }

    ring1_ctx.return_value = (int)regs->eax;
    ring1_ctx.valid = 0;

    driver_domain_deactivate();

    __asm__ volatile(
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        ::: "eax"
    );

    /* Restore saved kernel ESP/EBP and jump to resume.
     * This completely abandons the ISR stack frame. */
    uint32_t saved_esp = ring1_ctx.esp;
    uint32_t saved_ebp = ring1_ctx.ebp;
    int retval = ring1_ctx.return_value;

    __asm__ volatile(
        "mov %0, %%esp\n\t"
        "mov %1, %%ebp\n\t"
        "mov %2, %%eax\n\t"
        "sti\n\t"
        "ret\n\t"
        :
        : "r"(saved_esp), "r"(saved_ebp), "r"(retval)
        : "memory"
    );
    __builtin_unreachable();
}

void driver_isolation_install_handlers(void)
{
    register_interrupt_handler(129, ring1_service_handler);
    register_interrupt_handler(130, ring1_return_handler);
    serial_puts("[ISOLATION] Installed INT 0x81 (service) and INT 0x82 (return) handlers\n");
}

/*
 * Helper that performs the actual Ring 1 transition.
 * This is noinline so it has its own stack frame with a real return address.
 * When ring1_return_handler restores ESP/EBP and does RET, it returns
 * from this function back to driver_domain_exec with the result in EAX.
 */
static int __attribute__((noinline)) ring1_dispatch(driver_domain_t *domain,
                                                     int (*func)(void *arg), void *arg)
{
    ring1_func = func;
    ring1_arg = arg;

    ring1_ctx.valid = 1;
    ring1_ctx.return_value = -1;

    /* Save the current ESP and EBP.
     * When INT 0x82 fires, ring1_return_handler will:
     *   1. Restore these ESP/EBP values
     *   2. Put the return value in EAX
     *   3. Execute RET, which pops the return address that the compiler
     *      pushed when calling this function, returning to driver_domain_exec */
    __asm__ volatile(
        "mov %%esp, %0\n\t"
        "mov %%ebp, %1\n\t"
        : "=m"(ring1_ctx.esp), "=m"(ring1_ctx.ebp)
        :
        : "memory"
    );

    uint32_t r1_cs = GDT_DRIVER_CODE_SEGMENT | RING_DRIVER;
    uint32_t r1_ds = GDT_DRIVER_DATA_SEGMENT | RING_DRIVER;
    uint32_t r1_esp = domain->stack_top;
    uint32_t r1_eip = (uint32_t)&ring1_trampoline;

    serial_puts("[ISOLATION] Entering Ring 1\n");

    /* IRET to Ring 1 - does not return normally.
     * Control comes back via INT 0x82 -> ring1_return_handler
     * which restores ESP/EBP and RETs from this function. */
    ring1_enter(r1_cs, r1_ds, r1_esp, r1_eip);

    return -1;
}

int driver_domain_exec(driver_domain_t *domain, int (*func)(void *arg), void *arg)
{
    if (!domain || !domain->active || !func) return -1;

    driver_domain_activate(domain);

    int result;

    if (domain->isolation_level == DRIVER_ISOLATION_RING1) {
        result = ring1_dispatch(domain, func, arg);
    } else {
        result = func(arg);
    }

    driver_domain_deactivate();
    return result;
}

uint32_t driver_kernel_service(uint32_t service_id, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (current_domain) {
        current_domain->kernel_calls++;
    }

    switch (service_id) {
        case DRIVER_SVC_ALLOC_MEM: {
            void *ptr = kmalloc(arg1);
            return (uint32_t)ptr;
        }

        case DRIVER_SVC_FREE_MEM: {
            kfree((void *)arg1);
            return 0;
        }

        case DRIVER_SVC_LOG: {
            if (arg1) {
                serial_puts("[DRV:");
                if (current_domain) serial_puts(current_domain->name);
                serial_puts("] ");
                serial_puts((const char *)arg1);
            }
            return 0;
        }

        case DRIVER_SVC_PORT_IN: {
            if (current_domain) current_domain->total_io_ops++;
            if (current_domain && current_domain->isolation_level == DRIVER_ISOLATION_RING1) {
                uint16_t port = (uint16_t)arg1;
                if (current_domain->iopb[port / 8] & (1 << (port % 8))) {
                    current_domain->io_violations++;
                    serial_puts("[ISOLATION] I/O violation: port read denied\n");
                    return 0xFFFFFFFF;
                }
            }
            switch (arg2) {
                case 1: return inb((uint16_t)arg1);
                case 2: return inw((uint16_t)arg1);
                case 4: return inl((uint16_t)arg1);
                default: return 0;
            }
        }

        case DRIVER_SVC_PORT_OUT: {
            if (current_domain) current_domain->total_io_ops++;
            if (current_domain && current_domain->isolation_level == DRIVER_ISOLATION_RING1) {
                uint16_t port = (uint16_t)arg1;
                if (current_domain->iopb[port / 8] & (1 << (port % 8))) {
                    current_domain->io_violations++;
                    serial_puts("[ISOLATION] I/O violation: port write denied\n");
                    return (uint32_t)-1;
                }
            }
            switch (arg3) {
                case 1: outb((uint16_t)arg1, (uint8_t)arg2); break;
                case 2: outw((uint16_t)arg1, (uint16_t)arg2); break;
                case 4: outl((uint16_t)arg1, arg2); break;
            }
            return 0;
        }

        case DRIVER_SVC_REGISTER_IRQ:
        case DRIVER_SVC_UNREGISTER_IRQ:
        case DRIVER_SVC_DMA_ALLOC:
        case DRIVER_SVC_DMA_FREE:
        case DRIVER_SVC_MAP_MMIO:
        case DRIVER_SVC_PCI_READ:
        case DRIVER_SVC_PCI_WRITE:
            /* Placeholder for future implementation */
            return 0;

        default:
            serial_puts("[ISOLATION] Unknown kernel service\n");
            return (uint32_t)-1;
    }
}

driver_domain_t *driver_domain_get(int id)
{
    for (int i = 0; i < MAX_DRIVER_DOMAINS; i++) {
        if (domains[i].active && domains[i].id == id) {
            return &domains[i];
        }
    }
    return NULL;
}

driver_domain_t *driver_domain_find(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; i < MAX_DRIVER_DOMAINS; i++) {
        if (domains[i].active && domains[i].name &&
            strcmp(domains[i].name, name) == 0) {
            return &domains[i];
        }
    }
    return NULL;
}

uint32_t driver_domain_count(void)
{
    uint32_t count = 0;
    for (int i = 0; i < MAX_DRIVER_DOMAINS; i++) {
        if (domains[i].active) count++;
    }
    return count;
}

driver_domain_t *driver_domain_get_by_index(uint32_t index)
{
    uint32_t count = 0;
    for (int i = 0; i < MAX_DRIVER_DOMAINS; i++) {
        if (domains[i].active) {
            if (count == index) return &domains[i];
            count++;
        }
    }
    return NULL;
}

driver_domain_t *driver_domain_current(void)
{
    return current_domain;
}
