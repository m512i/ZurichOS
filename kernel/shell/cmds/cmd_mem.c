/* Shell Commands - Memory Category
 * mem, hexdump, peek, poke, alloc, memtest
 */

#include <shell/builtins.h>
#include <kernel/shell.h>
#include <kernel/kernel.h>
#include <drivers/vga.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <string.h>

extern uint32_t _kernel_start;
extern uint32_t _kernel_end;
extern uint32_t _kernel_end_phys;

void cmd_mem(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    uint32_t total = pmm_get_total_memory() / (1024 * 1024);
    uint32_t used = pmm_get_used_memory() / (1024 * 1024);
    uint32_t free_mem = pmm_get_free_memory() / (1024 * 1024);
    
    vga_puts("Memory Information:\n");
    vga_puts("  Total: ");
    vga_put_dec(total);
    vga_puts(" MB\n");
    vga_puts("  Used:  ");
    vga_put_dec(used);
    vga_puts(" MB\n");
    vga_puts("  Free:  ");
    vga_put_dec(free_mem);
    vga_puts(" MB\n");
}

void cmd_free(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    uint32_t total_kb = pmm_get_total_memory() / 1024;
    uint32_t used_kb = pmm_get_used_memory() / 1024;
    uint32_t free_kb = pmm_get_free_memory() / 1024;
    
    uint32_t heap_allocs, heap_frees, heap_current, heap_bytes, heap_peak_allocs, heap_peak_bytes;
    heap_get_stats(&heap_allocs, &heap_frees, &heap_current, &heap_bytes, &heap_peak_allocs, &heap_peak_bytes);
    
    vga_puts("              total        used        free\n");
    vga_puts("Mem:    ");
    
    if (total_kb < 10000) vga_puts(" ");
    if (total_kb < 1000) vga_puts(" ");
    vga_put_dec(total_kb);
    vga_puts(" KB   ");
    
    if (used_kb < 10000) vga_puts(" ");
    if (used_kb < 1000) vga_puts(" ");
    vga_put_dec(used_kb);
    vga_puts(" KB   ");
    
    if (free_kb < 10000) vga_puts(" ");
    if (free_kb < 1000) vga_puts(" ");
    vga_put_dec(free_kb);
    vga_puts(" KB\n");
    
    vga_puts("Heap:         ");
    vga_put_dec(heap_current);
    vga_puts(" allocs, ");
    vga_put_dec(heap_bytes);
    vga_puts(" bytes in use\n");
}

void cmd_hexdump(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: hexdump <addr> [length]\n");
        vga_puts("Example: hexdump 0xC0100000 64\n");
        return;
    }
    
    uint32_t addr;
    if (strcmp(argv[1], "kernel_start") == 0 || strcmp(argv[1], "_kernel_start") == 0) {
        addr = (uint32_t)&_kernel_start;
    } else if (strcmp(argv[1], "kernel_end") == 0 || strcmp(argv[1], "_kernel_end") == 0) {
        addr = (uint32_t)&_kernel_end;
    } else if (strcmp(argv[1], "vga") == 0) {
        addr = 0xC00B8000;
    } else {
        addr = shell_parse_hex(argv[1]);
    }
    
    if (addr < 0x1000 || (addr >= 0x01000000 && addr < 0xC0000000)) {
        vga_puts("Error: Invalid address. Use 0xC0xxxxxx for kernel space.\n");
        return;
    }
    
    uint32_t len = (argc >= 3) ? shell_parse_dec(argv[2]) : 64;
    if (len > 256) len = 256;
    
    uint8_t *ptr = (uint8_t *)addr;
    
    for (uint32_t i = 0; i < len; i += 16) {
        vga_put_hex(addr + i);
        vga_puts(": ");
        
        for (uint32_t j = 0; j < 16 && (i + j) < len; j++) {
            uint8_t b = ptr[i + j];
            vga_putchar("0123456789ABCDEF"[b >> 4]);
            vga_putchar("0123456789ABCDEF"[b & 0xF]);
            vga_putchar(' ');
        }
        
        vga_puts(" |");
        for (uint32_t j = 0; j < 16 && (i + j) < len; j++) {
            uint8_t b = ptr[i + j];
            vga_putchar((b >= 32 && b < 127) ? b : '.');
        }
        vga_puts("|\n");
    }
}

void cmd_peek(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: peek <addr>\n");
        return;
    }
    
    uint32_t addr = shell_parse_hex(argv[1]);
    
    if (addr < 0x1000 || (addr >= 0x01000000 && addr < 0xC0000000)) {
        vga_puts("Error: Invalid address. Use 0xC0xxxxxx for kernel space.\n");
        return;
    }
    
    uint32_t val = *(volatile uint32_t *)addr;
    
    vga_puts("[");
    vga_put_hex(addr);
    vga_puts("] = ");
    vga_put_hex(val);
    vga_puts("\n");
}

void cmd_poke(int argc, char **argv)
{
    if (argc < 3) {
        vga_puts("Usage: poke <addr> <value>\n");
        return;
    }
    
    uint32_t addr = shell_parse_hex(argv[1]);
    uint32_t val = shell_parse_hex(argv[2]);
    
    if (addr < 0x1000 || (addr >= 0x01000000 && addr < 0xC0000000)) {
        vga_puts("Error: Invalid address. Use 0xC0xxxxxx for kernel space.\n");
        return;
    }
    
    *(volatile uint32_t *)addr = val;
    
    vga_puts("Wrote ");
    vga_put_hex(val);
    vga_puts(" to ");
    vga_put_hex(addr);
    vga_puts("\n");
}

void cmd_alloc(int argc, char **argv)
{
    if (argc < 2) {
        vga_puts("Usage: alloc <size>\n");
        vga_puts("Example: alloc 64 (allocates 64 bytes)\n");
        return;
    }
    
    uint32_t size;
    if (argv[1][0] == '0' && (argv[1][1] == 'x' || argv[1][1] == 'X')) {
        size = shell_parse_hex(argv[1]);
    } else {
        size = shell_parse_dec(argv[1]);
    }
    
    if (size == 0 || size > 0x100000) {
        vga_puts("Error: Invalid size (1 - 1048576 bytes)\n");
        return;
    }
    
    void *ptr = kmalloc(size);
    
    if (ptr) {
        vga_puts("Allocated ");
        vga_put_dec(size);
        vga_puts(" bytes at ");
        vga_put_hex((uint32_t)ptr);
        vga_puts("\n");
    } else {
        vga_puts("Allocation failed!\n");
    }
}

void cmd_memtest(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("Memory Subsystem Tests\n");
    vga_puts("======================\n\n");
    
    vga_puts("Test 1: PMM Allocation/Free Cycles\n");
    vga_puts("-----------------------------------\n");
    
    uint32_t free_before = pmm_get_free_memory();
    vga_puts("Free memory before: ");
    vga_put_dec(free_before / 1024);
    vga_puts(" KB\n");
    
    uint32_t frames[10];
    int alloc_count = 0;
    
    vga_puts("Allocating 10 frames... ");
    for (int i = 0; i < 10; i++) {
        frames[i] = pmm_alloc_frame();
        if (frames[i] != 0) {
            alloc_count++;
        }
    }
    
    if (alloc_count == 10) {
        vga_puts("OK\n");
    } else {
        vga_puts("PARTIAL (");
        vga_put_dec(alloc_count);
        vga_puts("/10)\n");
    }
    
    uint32_t free_after_alloc = pmm_get_free_memory();
    vga_puts("Free memory after alloc: ");
    vga_put_dec(free_after_alloc / 1024);
    vga_puts(" KB (");
    vga_put_dec((free_before - free_after_alloc) / 1024);
    vga_puts(" KB used)\n");
    
    vga_puts("Freeing 10 frames... ");
    for (int i = 0; i < alloc_count; i++) {
        pmm_free_frame(frames[i]);
    }
    vga_puts("OK\n");
    
    uint32_t free_after_free = pmm_get_free_memory();
    vga_puts("Free memory after free: ");
    vga_put_dec(free_after_free / 1024);
    vga_puts(" KB\n");
    
    if (free_after_free == free_before) {
        vga_puts("Result: PASS (memory fully recovered)\n");
    } else {
        vga_puts("Result: WARN (");
        vga_put_dec((free_before - free_after_free) / 1024);
        vga_puts(" KB leak)\n");
    }
    
    vga_puts("\nTest 2: VMM Virtual Mapping\n");
    vga_puts("---------------------------\n");
    
    uint32_t test_virt = 0;
    for (uint32_t addr = 0xE0000000; addr < 0xF0000000; addr += 0x1000) {
        if (!vmm_is_mapped(addr)) {
            test_virt = addr;
            break;
        }
    }
    
    if (test_virt == 0) {
        vga_puts("Could not find unmapped address\n");
        return;
    }
    
    uint32_t test_phys = pmm_alloc_frame();
    
    if (test_phys == 0) {
        vga_puts("Failed to allocate test frame\n");
        return;
    }
    
    vga_puts("Test virtual addr: 0x");
    vga_put_hex(test_virt);
    vga_puts("\n");
    vga_puts("Test physical addr: 0x");
    vga_put_hex(test_phys);
    vga_puts("\n");
    
    vga_puts("Before mapping - is_mapped: ");
    vga_puts(vmm_is_mapped(test_virt) ? "yes" : "no");
    vga_puts("\n");
    
    vga_puts("Mapping page... ");
    vmm_map_page(test_virt, test_phys, PAGE_PRESENT | PAGE_WRITE);
    vga_puts("OK\n");
    
    vga_puts("After mapping - is_mapped: ");
    vga_puts(vmm_is_mapped(test_virt) ? "yes" : "no");
    vga_puts("\n");
    
    uint32_t retrieved_phys = vmm_get_physical(test_virt);
    vga_puts("Retrieved physical: 0x");
    vga_put_hex(retrieved_phys);
    vga_puts("\n");
    
    if (retrieved_phys == test_phys) {
        vga_puts("Result: PASS (physical address matches)\n");
    } else {
        vga_puts("Result: FAIL (address mismatch)\n");
    }
    
    vga_puts("\nTest 3: Memory Access Through Mapping\n");
    vga_puts("--------------------------------------\n");
    
    volatile uint32_t *test_ptr = (volatile uint32_t *)test_virt;
    uint32_t test_pattern = 0xDEADBEEF;
    
    vga_puts("Writing 0xDEADBEEF... ");
    *test_ptr = test_pattern;
    vga_puts("OK\n");
    
    vga_puts("Reading back... ");
    uint32_t read_val = *test_ptr;
    vga_puts("0x");
    vga_put_hex(read_val);
    vga_puts("\n");
    
    if (read_val == test_pattern) {
        vga_puts("Result: PASS (read matches write)\n");
    } else {
        vga_puts("Result: FAIL (data corruption)\n");
    }
    
    vga_puts("\nTest 4: Recursive Page Table Access\n");
    vga_puts("------------------------------------\n");
    
    /* With recursive mapping, last PDE points to PD itself
     * PD is at 0xFFFFF000, PT for any address can be accessed via 0xFFC00000 + (pde_idx * 0x1000)
     */
    uint32_t *page_dir = vmm_get_current_pagedir();
    vga_puts("Page directory at: 0x");
    vga_put_hex((uint32_t)page_dir);
    vga_puts("\n");
    
    uint32_t pde_idx = test_virt >> 22;
    uint32_t pde = page_dir[pde_idx];
    vga_puts("PDE[");
    vga_put_dec(pde_idx);
    vga_puts("] = 0x");
    vga_put_hex(pde);
    vga_puts(" (");
    vga_puts((pde & PAGE_PRESENT) ? "P" : "-");
    vga_puts((pde & PAGE_WRITE) ? "W" : "-");
    vga_puts((pde & PAGE_USER) ? "U" : "-");
    vga_puts(")\n");
    
    uint32_t *page_table = (uint32_t *)(0xFFC00000 + (pde_idx * 0x1000));
    uint32_t pte_idx = (test_virt >> 12) & 0x3FF;
    uint32_t pte = page_table[pte_idx];
    vga_puts("PTE[");
    vga_put_dec(pte_idx);
    vga_puts("] = 0x");
    vga_put_hex(pte);
    vga_puts(" (");
    vga_puts((pte & PAGE_PRESENT) ? "P" : "-");
    vga_puts((pte & PAGE_WRITE) ? "W" : "-");
    vga_puts((pte & PAGE_USER) ? "U" : "-");
    vga_puts(")\n");
    
    if ((pte & 0xFFFFF000) == test_phys) {
        vga_puts("Result: PASS (PTE points to correct frame)\n");
    } else {
        vga_puts("Result: FAIL (PTE mismatch)\n");
    }
    
    vga_puts("\nCleanup:\n");
    vga_puts("Unmapping test page... ");
    vmm_unmap_page(test_virt);
    vga_puts("OK\n");
    
    vga_puts("Freeing test frame... ");
    pmm_free_frame(test_phys);
    vga_puts("OK\n");
    
    vga_puts("\n=== All tests complete ===\n");
}

void cmd_heapstats(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    uint32_t allocs, frees, current, bytes, peak_allocs, peak_bytes;
    heap_get_stats(&allocs, &frees, &current, &bytes, &peak_allocs, &peak_bytes);
    
    vga_puts("Heap Statistics:\n");
    vga_puts("================\n");
    
    vga_puts("  Total allocations:   ");
    vga_put_dec(allocs);
    vga_puts("\n");
    
    vga_puts("  Total frees:         ");
    vga_put_dec(frees);
    vga_puts("\n");
    
    vga_puts("  Current allocations: ");
    vga_put_dec(current);
    vga_puts("\n");
    
    vga_puts("  Bytes in use:        ");
    vga_put_dec(bytes);
    vga_puts("\n");
    
    vga_puts("  Peak allocations:    ");
    vga_put_dec(peak_allocs);
    vga_puts("\n");
    
    vga_puts("  Peak bytes:          ");
    vga_put_dec(peak_bytes);
    vga_puts("\n");
    
    int leaks = heap_check_leaks();
    if (leaks > 0) {
        vga_puts("\n  WARNING: ");
        vga_put_dec(leaks);
        vga_puts(" potential leak(s) detected!\n");
    } else {
        vga_puts("\n  No leaks detected.\n");
    }
}

void cmd_leaktest(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("Leak Detection Test\n");
    vga_puts("===================\n\n");
    
    uint32_t before_allocs, before_frees, dummy1, dummy2, dummy3, dummy4;
    heap_get_stats(&before_allocs, &before_frees, &dummy1, &dummy2, &dummy3, &dummy4);
    
    vga_puts("Test 1: Allocate and free (no leak)\n");
    void *p1 = kmalloc(100);
    void *p2 = kmalloc(200);
    void *p3 = kmalloc(300);
    kfree(p1);
    kfree(p2);
    kfree(p3);
    
    int leaks = heap_check_leaks();
    vga_puts("  Leaks after cleanup: ");
    vga_put_dec(leaks);
    vga_puts(leaks == 0 ? " - PASS\n" : " - (expected from prior allocs)\n");
    
    vga_puts("\nTest 2: Deliberate leak (allocate without free)\n");
    uint32_t before_current;
    heap_get_stats(&dummy1, &dummy2, &before_current, &dummy3, &dummy4, &dummy4);
    
    void *leaked = kmalloc(64);
    (void)leaked;
    
    uint32_t after_current;
    heap_get_stats(&dummy1, &dummy2, &after_current, &dummy3, &dummy4, &dummy4);
    
    if (after_current > before_current) {
        vga_puts("  Leak detected: current allocs increased - PASS\n");
    } else {
        vga_puts("  Leak NOT detected - FAIL\n");
    }
    
    vga_puts("\nNote: The deliberate leak remains for demonstration.\n");
    vga_puts("Run 'heapstats' to see current allocation count.\n");
}
