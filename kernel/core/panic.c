/* Kernel Panic Handler */

#include <kernel/kernel.h>
#include <kernel/symbols.h>
#include <drivers/vga.h>
#include <drivers/serial.h>
#include <stdint.h>

extern uint32_t _kernel_start;
extern uint32_t _kernel_end;

typedef struct stack_frame {
    struct stack_frame *ebp;
    uint32_t eip;
} stack_frame_t;

static void panic_put_hex(uint32_t val)
{
    const char *hex = "0123456789ABCDEF";
    vga_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        vga_putchar(hex[(val >> i) & 0xF]);
    }
}

static void serial_put_hex(uint32_t val)
{
    const char *hex = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        char c = hex[(val >> i) & 0xF];
        serial_puts((char[]){c, '\0'});
    }
}

static void dump_registers(void)
{
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp;
    uint32_t eflags, cr0, cr2, cr3;
    
    __asm__ volatile(
        "movl %%eax, %0\n"
        "movl %%ebx, %1\n"
        "movl %%ecx, %2\n"
        "movl %%edx, %3\n"
        : "=m"(eax), "=m"(ebx), "=m"(ecx), "=m"(edx)
    );
    
    __asm__ volatile(
        "movl %%esi, %0\n"
        "movl %%edi, %1\n"
        "movl %%ebp, %2\n"
        "movl %%esp, %3\n"
        : "=m"(esi), "=m"(edi), "=m"(ebp), "=m"(esp)
    );
    
    __asm__ volatile("pushfl; popl %0" : "=r"(eflags));
    __asm__ volatile("movl %%cr0, %0" : "=r"(cr0));
    __asm__ volatile("movl %%cr2, %0" : "=r"(cr2));
    __asm__ volatile("movl %%cr3, %0" : "=r"(cr3));
    
    vga_puts("\nRegisters:\n");
    vga_puts("  EAX="); panic_put_hex(eax);
    vga_puts("  EBX="); panic_put_hex(ebx);
    vga_puts("  ECX="); panic_put_hex(ecx);
    vga_puts("  EDX="); panic_put_hex(edx);
    vga_puts("\n");
    vga_puts("  ESI="); panic_put_hex(esi);
    vga_puts("  EDI="); panic_put_hex(edi);
    vga_puts("  EBP="); panic_put_hex(ebp);
    vga_puts("  ESP="); panic_put_hex(esp);
    vga_puts("\n");
    vga_puts("  EFLAGS="); panic_put_hex(eflags);
    vga_puts("\n");
    vga_puts("  CR0="); panic_put_hex(cr0);
    vga_puts("  CR2="); panic_put_hex(cr2);
    vga_puts("  CR3="); panic_put_hex(cr3);
    vga_puts("\n");
    
    serial_puts("\nRegisters:\n");
    serial_puts("  EAX="); serial_put_hex(eax);
    serial_puts("  EBX="); serial_put_hex(ebx);
    serial_puts("  ECX="); serial_put_hex(ecx);
    serial_puts("  EDX="); serial_put_hex(edx);
    serial_puts("\n");
    serial_puts("  CR2="); serial_put_hex(cr2);
    serial_puts(" (page fault address)\n");
}

static void print_stack_trace(void)
{
    stack_frame_t *frame;
    uint32_t kernel_start = (uint32_t)&_kernel_start;
    uint32_t kernel_end = (uint32_t)&_kernel_end;
    
    __asm__ volatile("movl %%ebp, %0" : "=r"(frame));
    
    vga_puts("\nStack Trace:\n");
    serial_puts("\nStack Trace:\n");
    
    int depth = 0;
    const int max_depth = 20;
    
    while (frame && depth < max_depth) {
        uint32_t frame_addr = (uint32_t)frame;
        if (frame_addr < 0xC0000000 || frame_addr > 0xFFFFFFFF) {
            break;
        }
        
        uint32_t eip = frame->eip;
        
        if (eip >= kernel_start && eip < kernel_end) {
            const char *name = symbols_lookup(eip);
            
            vga_puts("  ["); 
            vga_put_dec(depth);
            vga_puts("] ");
            panic_put_hex(eip);
            if (name) {
                vga_puts(" <");
                vga_puts(name);
                vga_puts(">");
            }
            vga_puts("\n");
            
            serial_puts("  [");
            serial_put_hex(depth);
            serial_puts("] ");
            serial_put_hex(eip);
            if (name) {
                serial_puts(" <");
                serial_puts(name);
                serial_puts(">");
            }
            serial_puts("\n");
        } else if (eip != 0) {
            vga_puts("  [");
            vga_put_dec(depth);
            vga_puts("] ");
            panic_put_hex(eip);
            vga_puts(" (outside kernel)\n");
        }
        
        stack_frame_t *next = frame->ebp;
        if ((uint32_t)next <= frame_addr) {
            break;  
        }
        frame = next;
        depth++;
    }
    
    if (depth == 0) {
        vga_puts("  (no stack frames found)\n");
    }
}

static void dump_stack(void)
{
    uint32_t esp;
    __asm__ volatile("movl %%esp, %0" : "=r"(esp));
    
    vga_puts("\nStack Dump (ESP=");
    panic_put_hex(esp);
    vga_puts("):\n");
    
    uint32_t *stack = (uint32_t *)esp;
    for (int i = 0; i < 8; i++) {
        vga_puts("  ");
        panic_put_hex((uint32_t)&stack[i]);
        vga_puts(": ");
        panic_put_hex(stack[i]);
        vga_puts("\n");
    }
}

void panic(const char *message)
{
    cli();
    
    vga_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    
    vga_puts("\n");
    vga_puts("================================================================================");
    vga_puts("                           *** KERNEL PANIC ***                                 ");
    vga_puts("================================================================================");
    vga_puts("\n");
    
    vga_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    vga_puts("Error: ");
    vga_puts(message);
    vga_puts("\n");
    
    serial_puts("\n========== KERNEL PANIC ==========\n");
    serial_puts("Error: ");
    serial_puts(message);
    serial_puts("\n");
    
    dump_registers();
    print_stack_trace();
    dump_stack();
    
    vga_puts("\n");
    vga_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    vga_puts("System halted. Please reboot.\n");
    
    serial_puts("\n[PANIC] System halted\n");
    
    for (;;) {
        hlt();
    }
}

void panic_with_regs(const char *message, uint32_t eip, uint32_t cs, 
                     uint32_t eflags, uint32_t err_code)
{
    cli();
    
    vga_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    vga_puts("\n");
    vga_puts("================================================================================");
    vga_puts("                           *** KERNEL PANIC ***                                 ");
    vga_puts("================================================================================");
    vga_puts("\n");
    
    vga_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    vga_puts("Exception: ");
    vga_puts(message);
    vga_puts("\n\n");
    
    vga_puts("Fault Address (EIP): ");
    panic_put_hex(eip);
    vga_puts("\n");
    vga_puts("Code Segment (CS):   ");
    panic_put_hex(cs);
    vga_puts("\n");
    vga_puts("EFLAGS:              ");
    panic_put_hex(eflags);
    vga_puts("\n");
    vga_puts("Error Code:          ");
    panic_put_hex(err_code);
    vga_puts("\n");
    
    uint32_t cr2;
    __asm__ volatile("movl %%cr2, %0" : "=r"(cr2));
    vga_puts("CR2 (fault addr):    ");
    panic_put_hex(cr2);
    vga_puts("\n");
    
    serial_puts("\n========== KERNEL PANIC ==========\n");
    serial_puts("Exception: ");
    serial_puts(message);
    serial_puts("\nEIP="); serial_put_hex(eip);
    serial_puts(" CR2="); serial_put_hex(cr2);
    serial_puts("\n");
    
    print_stack_trace();
    
    vga_puts("\n");
    vga_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    vga_puts("System halted. Please reboot.\n");
    
    for (;;) {
        hlt();
    }
}
