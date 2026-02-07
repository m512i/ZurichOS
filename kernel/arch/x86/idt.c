/* Interrupt Descriptor Table (IDT)
 * Handles CPU exceptions and hardware interrupts
 */

#include <kernel/kernel.h>
#include <arch/x86/idt.h>
#include <drivers/vga.h>
#include <drivers/serial.h>
#include <apic/lapic.h>

static int using_apic = 0;
static idt_entry_t idt_entries[IDT_ENTRIES];
static idt_ptr_t idt_ptr;
static interrupt_handler_t interrupt_handlers[IDT_ENTRIES];

extern void idt_flush(uint32_t idt_ptr);

static const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Security Exception",
    "Reserved"
};

static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags)
{
    idt_entries[num].base_low = base & 0xFFFF;
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].selector = sel;
    idt_entries[num].always0 = 0;
    idt_entries[num].flags = flags;
}

/* External ISR stubs from assembly */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);
extern void isr128(void);  /* Syscall INT 0x80 */
extern void isr129(void);  /* Driver service INT 0x81 */
extern void isr130(void);  /* Driver return INT 0x82 */

/* External IRQ stubs from assembly */
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

static void pic_remap(void)
{
    uint8_t a1 = inb(0x21);
    uint8_t a2 = inb(0xA1);

    /* Start init sequence */
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    /* Set vector offsets */
    outb(0x21, 0x20);  /* Master PIC: IRQ 0-7 -> INT 32-39 */
    outb(0xA1, 0x28);  /* Slave PIC: IRQ 8-15 -> INT 40-47 */

    /* Tell Master about Slave at IRQ2 */
    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    /* 8086 mode */
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    /* Restore masks */
    outb(0x21, a1);
    outb(0xA1, a2);
}

/* Disable the legacy 8259 PIC (when using APIC) */
void pic_disable(void)
{
    /* Mask all interrupts on both PICs */
    outb(0xA1, 0xFF);
    outb(0x21, 0xFF);
}

void idt_set_apic_mode(int enabled)
{
    using_apic = enabled;
}

int idt_is_apic_mode(void)
{
    return using_apic;
}

void idt_init(void)
{
    idt_ptr.limit = sizeof(idt_entry_t) * IDT_ENTRIES - 1;
    idt_ptr.base = (uint32_t)&idt_entries;

    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
        interrupt_handlers[i] = 0;
    }

    /*install cpu exception handlers (ISR 0-31) */
    idt_set_gate(0, (uint32_t)isr0, 0x08, IDT_KERNEL_INT);
    idt_set_gate(1, (uint32_t)isr1, 0x08, IDT_KERNEL_INT);
    idt_set_gate(2, (uint32_t)isr2, 0x08, IDT_KERNEL_INT);
    idt_set_gate(3, (uint32_t)isr3, 0x08, IDT_KERNEL_INT);
    idt_set_gate(4, (uint32_t)isr4, 0x08, IDT_KERNEL_INT);
    idt_set_gate(5, (uint32_t)isr5, 0x08, IDT_KERNEL_INT);
    idt_set_gate(6, (uint32_t)isr6, 0x08, IDT_KERNEL_INT);
    idt_set_gate(7, (uint32_t)isr7, 0x08, IDT_KERNEL_INT);
    idt_set_gate(8, (uint32_t)isr8, 0x08, IDT_KERNEL_INT);
    idt_set_gate(9, (uint32_t)isr9, 0x08, IDT_KERNEL_INT);
    idt_set_gate(10, (uint32_t)isr10, 0x08, IDT_KERNEL_INT);
    idt_set_gate(11, (uint32_t)isr11, 0x08, IDT_KERNEL_INT);
    idt_set_gate(12, (uint32_t)isr12, 0x08, IDT_KERNEL_INT);
    idt_set_gate(13, (uint32_t)isr13, 0x08, IDT_KERNEL_INT);
    idt_set_gate(14, (uint32_t)isr14, 0x08, IDT_KERNEL_INT);
    idt_set_gate(15, (uint32_t)isr15, 0x08, IDT_KERNEL_INT);
    idt_set_gate(16, (uint32_t)isr16, 0x08, IDT_KERNEL_INT);
    idt_set_gate(17, (uint32_t)isr17, 0x08, IDT_KERNEL_INT);
    idt_set_gate(18, (uint32_t)isr18, 0x08, IDT_KERNEL_INT);
    idt_set_gate(19, (uint32_t)isr19, 0x08, IDT_KERNEL_INT);
    idt_set_gate(20, (uint32_t)isr20, 0x08, IDT_KERNEL_INT);
    idt_set_gate(21, (uint32_t)isr21, 0x08, IDT_KERNEL_INT);
    idt_set_gate(22, (uint32_t)isr22, 0x08, IDT_KERNEL_INT);
    idt_set_gate(23, (uint32_t)isr23, 0x08, IDT_KERNEL_INT);
    idt_set_gate(24, (uint32_t)isr24, 0x08, IDT_KERNEL_INT);
    idt_set_gate(25, (uint32_t)isr25, 0x08, IDT_KERNEL_INT);
    idt_set_gate(26, (uint32_t)isr26, 0x08, IDT_KERNEL_INT);
    idt_set_gate(27, (uint32_t)isr27, 0x08, IDT_KERNEL_INT);
    idt_set_gate(28, (uint32_t)isr28, 0x08, IDT_KERNEL_INT);
    idt_set_gate(29, (uint32_t)isr29, 0x08, IDT_KERNEL_INT);
    idt_set_gate(30, (uint32_t)isr30, 0x08, IDT_KERNEL_INT);
    idt_set_gate(31, (uint32_t)isr31, 0x08, IDT_KERNEL_INT);

    pic_remap();

    /*install irq handlers (IRQ 0-15 -> INT 32-47) */
    idt_set_gate(32, (uint32_t)irq0, 0x08, IDT_KERNEL_INT);
    idt_set_gate(33, (uint32_t)irq1, 0x08, IDT_KERNEL_INT);
    idt_set_gate(34, (uint32_t)irq2, 0x08, IDT_KERNEL_INT);
    idt_set_gate(35, (uint32_t)irq3, 0x08, IDT_KERNEL_INT);
    idt_set_gate(36, (uint32_t)irq4, 0x08, IDT_KERNEL_INT);
    idt_set_gate(37, (uint32_t)irq5, 0x08, IDT_KERNEL_INT);
    idt_set_gate(38, (uint32_t)irq6, 0x08, IDT_KERNEL_INT);
    idt_set_gate(39, (uint32_t)irq7, 0x08, IDT_KERNEL_INT);
    idt_set_gate(40, (uint32_t)irq8, 0x08, IDT_KERNEL_INT);
    idt_set_gate(41, (uint32_t)irq9, 0x08, IDT_KERNEL_INT);
    idt_set_gate(42, (uint32_t)irq10, 0x08, IDT_KERNEL_INT);
    idt_set_gate(43, (uint32_t)irq11, 0x08, IDT_KERNEL_INT);
    idt_set_gate(44, (uint32_t)irq12, 0x08, IDT_KERNEL_INT);
    idt_set_gate(45, (uint32_t)irq13, 0x08, IDT_KERNEL_INT);
    idt_set_gate(46, (uint32_t)irq14, 0x08, IDT_KERNEL_INT);
    idt_set_gate(47, (uint32_t)irq15, 0x08, IDT_KERNEL_INT);

    /* syscall interrupt (INT 0x80) - DPL=3 for user access */
    idt_set_gate(128, (uint32_t)isr128, 0x08, IDT_USER_INT);

    /* Driver service call (INT 0x81) - DPL=1 for Ring 1 drivers */
    idt_set_gate(129, (uint32_t)isr129, 0x08, IDT_DRIVER_INT);

    /* Driver return (INT 0x82) - DPL=1 for Ring 1 drivers */
    idt_set_gate(130, (uint32_t)isr130, 0x08, IDT_DRIVER_INT);

    idt_flush((uint32_t)&idt_ptr);
}

void idt_set_handler(uint8_t num, uint32_t handler, uint16_t sel, uint8_t flags)
{
    idt_set_gate(num, handler, sel, flags);
}

void register_interrupt_handler(uint8_t n, interrupt_handler_t handler)
{
    interrupt_handlers[n] = handler;
}

void isr_handler(registers_t *regs)
{
    if (interrupt_handlers[regs->int_no] != 0) {
        interrupt_handlers[regs->int_no](regs);
    } else {
        const char *msg = "Unknown Exception";
        if (regs->int_no < 32) {
            msg = exception_messages[regs->int_no];
        }
        panic_with_regs(msg, regs->eip, regs->cs, regs->eflags, regs->err_code);
    }
}

void irq_handler(registers_t *regs)
{
    if (interrupt_handlers[regs->int_no] != 0) {
        interrupt_handlers[regs->int_no](regs);
    }
    
    if (using_apic) {
        lapic_eoi();
    } else {
        if (regs->int_no >= 40) {
            outb(0xA0, 0x20);
        }
        outb(0x20, 0x20);
    }
}
