#ifndef _ARCH_X86_IDT_H
#define _ARCH_X86_IDT_H

#include <stdint.h>

#define IDT_ENTRIES 256

#define IDT_GATE_TASK       0x05
#define IDT_GATE_INT16      0x06
#define IDT_GATE_TRAP16     0x07
#define IDT_GATE_INT32      0x0E
#define IDT_GATE_TRAP32     0x0F

#define IDT_FLAG_PRESENT    0x80
#define IDT_FLAG_RING0      0x00
#define IDT_FLAG_RING1      0x20
#define IDT_FLAG_RING3      0x60

#define IDT_KERNEL_INT      (IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_GATE_INT32)
#define IDT_DRIVER_INT      (IDT_FLAG_PRESENT | IDT_FLAG_RING1 | IDT_GATE_INT32)
#define IDT_USER_INT        (IDT_FLAG_PRESENT | IDT_FLAG_RING3 | IDT_GATE_INT32)

#define IRQ0    32
#define IRQ1    33
#define IRQ2    34
#define IRQ3    35
#define IRQ4    36
#define IRQ5    37
#define IRQ6    38
#define IRQ7    39
#define IRQ8    40
#define IRQ9    41
#define IRQ10   42
#define IRQ11   43
#define IRQ12   44
#define IRQ13   45
#define IRQ14   46
#define IRQ15   47

typedef struct idt_entry {
    uint16_t base_low;      
    uint16_t selector;      
    uint8_t  always0;       
    uint8_t  flags;         
    uint16_t base_high;     
} __attribute__((packed)) idt_entry_t;

typedef struct idt_ptr {
    uint16_t limit;         
    uint32_t base;          
} __attribute__((packed)) idt_ptr_t;

typedef struct registers {
    uint32_t ds;                                    
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; 
    uint32_t int_no, err_code;                      
    uint32_t eip, cs, eflags, useresp, ss;          
} registers_t;

typedef void (*interrupt_handler_t)(registers_t *);

void idt_init(void);
void idt_set_handler(uint8_t num, uint32_t handler, uint16_t sel, uint8_t flags);
void register_interrupt_handler(uint8_t n, interrupt_handler_t handler);

void isr_handler(registers_t *regs);
void irq_handler(registers_t *regs);

void pic_disable(void);

void idt_set_apic_mode(int enabled);
int idt_is_apic_mode(void);

#endif 
