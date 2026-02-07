/* Kernel Symbol Table
 * Maps addresses to function names for stack traces
 */

#include <kernel/symbols.h>
#include <kernel/kernel.h>
#include <string.h>

#define MAX_SYMBOLS 128

static ksym_t symbol_table[MAX_SYMBOLS];
static uint32_t symbol_count = 0;

extern uint32_t _kernel_start;
extern uint32_t _kernel_end;

extern void kernel_main(void);
extern void panic(const char *msg);
extern void shell_run(void);
extern void scheduler_init(void);
extern void schedule(void);
extern void task_block(uint32_t reason);
extern void task_unblock(void *task);
extern void mutex_lock(void *mutex);
extern void mutex_unlock(void *mutex);
extern void kmalloc(uint32_t size);
extern void kfree(void *ptr);
extern void vga_puts(const char *str);
extern void serial_puts(const char *str);
void symbols_init(void)
{
    symbol_count = 0;
    
    symbols_add((uint32_t)kernel_main, "kernel_main");
    symbols_add((uint32_t)panic, "panic");
    symbols_add((uint32_t)shell_run, "shell_run");
    symbols_add((uint32_t)scheduler_init, "scheduler_init");
    symbols_add((uint32_t)schedule, "schedule");
    symbols_add((uint32_t)task_block, "task_block");
    symbols_add((uint32_t)task_unblock, "task_unblock");
    symbols_add((uint32_t)mutex_lock, "mutex_lock");
    symbols_add((uint32_t)mutex_unlock, "mutex_unlock");
    symbols_add((uint32_t)kmalloc, "kmalloc");
    symbols_add((uint32_t)kfree, "kfree");
    symbols_add((uint32_t)vga_puts, "vga_puts");
    symbols_add((uint32_t)serial_puts, "serial_puts");
    
    for (uint32_t i = 0; i < symbol_count - 1; i++) {
        for (uint32_t j = i + 1; j < symbol_count; j++) {
            if (symbol_table[j].addr < symbol_table[i].addr) {
                ksym_t tmp = symbol_table[i];
                symbol_table[i] = symbol_table[j];
                symbol_table[j] = tmp;
            }
        }
    }
}

void symbols_add(uint32_t addr, const char *name)
{
    if (symbol_count >= MAX_SYMBOLS) return;
    
    symbol_table[symbol_count].addr = addr;
    symbol_table[symbol_count].name = name;
    symbol_count++;
}

const char *symbols_lookup(uint32_t addr)
{
    if (symbol_count == 0) return NULL;
    
    for (int i = symbol_count - 1; i >= 0; i--) {
        if (addr >= symbol_table[i].addr) {
            return symbol_table[i].name;
        }
    }
    
    return NULL;
}

uint32_t symbols_lookup_name(const char *name)
{
    for (uint32_t i = 0; i < symbol_count; i++) {
        if (strcmp(symbol_table[i].name, name) == 0) {
            return symbol_table[i].addr;
        }
    }
    return 0;
}
