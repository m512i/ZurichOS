/* ELF Loader
 * Full Ring 3 UM support
 */

#include <kernel/elf.h>
#include <kernel/scheduler.h>
#include <kernel/process.h>
#include <kernel/kernel.h>
#include <fs/vfs.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <drivers/serial.h>
#include <drivers/vga.h>
#include <string.h>
#include <arch/x86/gdt.h>
#include <syscall/syscall.h>

/* UM layout - must be below 0xC0000000 for Ring 3 access */
#define USER_STACK_TOP   0xBFFFF000
#define USER_STACK_SIZE  0x00010000

user_process_t *elf_load_from_memory(const uint8_t *data, uint32_t size)
{
    if (size < sizeof(elf_header_t)) {
        serial_puts("[ELF] File too small\n");
        return NULL;
    }
    
    elf_header_t *hdr = (elf_header_t *)data;
    
    if (hdr->e_ident[EI_MAG0] != 0x7F ||
        hdr->e_ident[EI_MAG1] != 'E' ||
        hdr->e_ident[EI_MAG2] != 'L' ||
        hdr->e_ident[EI_MAG3] != 'F') {
        serial_puts("[ELF] Invalid magic number\n");
        return NULL;
    }
    
    if (hdr->e_ident[EI_CLASS] != ELFCLASS32) {
        serial_puts("[ELF] Not 32-bit ELF\n");
        return NULL;
    }
    
    if (hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        serial_puts("[ELF] Not little-endian\n");
        return NULL;
    }
    
    if (hdr->e_machine != EM_386) {
        serial_puts("[ELF] Not i386\n");
        return NULL;
    }
    
    if (hdr->e_type != ET_EXEC) {
        serial_puts("[ELF] Not executable\n");
        return NULL;
    }
    
    serial_puts("[ELF] Valid ELF header\n");
    serial_puts("[ELF] Entry point: 0x");
    char hex[9];
    for (int k = 7; k >= 0; k--) {
        int nibble = (hdr->e_entry >> (k * 4)) & 0xF;
        hex[7-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    hex[8] = '\0';
    serial_puts(hex);
    serial_puts("\n");
    
    user_process_t *proc = (user_process_t *)kmalloc(sizeof(user_process_t));
    if (!proc) {
        return NULL;
    }
    
    memset(proc, 0, sizeof(user_process_t));
    strcpy(proc->name, "user");
    
    elf_phdr_t *phdrs = (elf_phdr_t *)(data + hdr->e_phoff);
    
    for (int i = 0; i < hdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            uint32_t vaddr = phdrs[i].p_vaddr;  
            uint32_t filesz = phdrs[i].p_filesz;
            uint32_t memsz = phdrs[i].p_memsz;
            uint32_t offset = phdrs[i].p_offset;
            
            serial_puts("[ELF] Loading segment to 0x");
            for (int k = 7; k >= 0; k--) {
                int nibble = (vaddr >> (k * 4)) & 0xF;
                hex[7-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
            }
            serial_puts(hex);
            serial_puts("\n");
            
            uint32_t page_start = vaddr & ~0xFFF;
            uint32_t page_end = (vaddr + memsz + 0xFFF) & ~0xFFF;
            
            serial_puts("[ELF] Mapping pages from 0x");
            for (int k = 7; k >= 0; k--) {
                int nibble = (page_start >> (k * 4)) & 0xF;
                hex[7-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
            }
            serial_puts(hex);
            serial_puts(" to 0x");
            for (int k = 7; k >= 0; k--) {
                int nibble = (page_end >> (k * 4)) & 0xF;
                hex[7-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
            }
            serial_puts(hex);
            serial_puts("\n");
            
            for (uint32_t addr = page_start; addr < page_end; addr += 0x1000) {
                uint32_t phys = pmm_alloc_frame();
                if (!phys) {
                    serial_puts("[ELF] Out of memory\n");
                    kfree(proc);
                    return NULL;
                }
                vmm_map_page(addr, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
            }
            
            if (filesz > 0) {
                memcpy((void *)vaddr, data + offset, filesz);
            }
            
            if (memsz > filesz) {
                memset((void *)(vaddr + filesz), 0, memsz - filesz);
            }
        }
    }
    
    proc->entry = hdr->e_entry;
    proc->init_array = 0;
    proc->init_array_size = 0;
    proc->fini_array = 0;
    proc->fini_array_size = 0;
    
    if (hdr->e_shoff != 0 && hdr->e_shnum > 0) {
        elf_shdr_t *shdrs = (elf_shdr_t *)(data + hdr->e_shoff);
        const char *shstrtab = NULL;
        
        if (hdr->e_shstrndx < hdr->e_shnum) {
            shstrtab = (const char *)(data + shdrs[hdr->e_shstrndx].sh_offset);
        }
        
        uint32_t ctors_start = 0xFFFFFFFF;
        uint32_t ctors_end = 0;
        uint32_t dtors_start = 0xFFFFFFFF;
        uint32_t dtors_end = 0;
        
        for (int i = 0; i < hdr->e_shnum; i++) {
            if (shdrs[i].sh_type == SHT_INIT_ARRAY) {
                proc->init_array = shdrs[i].sh_addr;
                proc->init_array_size = shdrs[i].sh_size;
                serial_puts("[ELF] Found .init_array\n");
            } else if (shdrs[i].sh_type == SHT_FINI_ARRAY) {
                proc->fini_array = shdrs[i].sh_addr;
                proc->fini_array_size = shdrs[i].sh_size;
                serial_puts("[ELF] Found .fini_array\n");
            }
            
            if (shstrtab && shdrs[i].sh_name != 0) {
                const char *name = shstrtab + shdrs[i].sh_name;
                if (name[0] == '.' && name[1] == 'c' && name[2] == 't' && 
                    name[3] == 'o' && name[4] == 'r' && name[5] == 's') {
                    if (shdrs[i].sh_addr < ctors_start) ctors_start = shdrs[i].sh_addr;
                    if (shdrs[i].sh_addr + shdrs[i].sh_size > ctors_end) 
                        ctors_end = shdrs[i].sh_addr + shdrs[i].sh_size;
                    serial_puts("[ELF] Found .ctors section\n");
                }
                if (name[0] == '.' && name[1] == 'd' && name[2] == 't' && 
                    name[3] == 'o' && name[4] == 'r' && name[5] == 's') {
                    if (shdrs[i].sh_addr < dtors_start) dtors_start = shdrs[i].sh_addr;
                    if (shdrs[i].sh_addr + shdrs[i].sh_size > dtors_end) 
                        dtors_end = shdrs[i].sh_addr + shdrs[i].sh_size;
                    serial_puts("[ELF] Found .dtors section\n");
                }
            }
        }
        
        if (proc->init_array == 0 && ctors_start != 0xFFFFFFFF) {
            proc->init_array = ctors_start;
            proc->init_array_size = ctors_end - ctors_start;
            serial_puts("[ELF] Using .ctors as init_array at 0x");
            for (int k = 7; k >= 0; k--) {
                int nibble = (ctors_start >> (k * 4)) & 0xF;
                hex[7-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
            }
            serial_puts(hex);
            serial_puts("\n");
        }
        
        if (proc->fini_array == 0 && dtors_start != 0xFFFFFFFF) {
            proc->fini_array = dtors_start;
            proc->fini_array_size = dtors_end - dtors_start;
            serial_puts("[ELF] Using .dtors as fini_array\n");
        }
    }
    
    /* Allocate user stack with USER flag
     * Stack grows DOWN, so we map from stack_base to stack_top
     * and set ESP to stack_top */
    uint32_t stack_base = USER_STACK_TOP - USER_STACK_SIZE;
    
    serial_puts("[ELF] Mapping user stack from 0x");
    for (int k = 7; k >= 0; k--) {
        int nibble = (stack_base >> (k * 4)) & 0xF;
        hex[7-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    serial_puts(hex);
    serial_puts(" to 0x");
    for (int k = 7; k >= 0; k--) {
        int nibble = (USER_STACK_TOP >> (k * 4)) & 0xF;
        hex[7-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    serial_puts(hex);
    serial_puts("\n");
    
    for (uint32_t addr = stack_base; addr < USER_STACK_TOP; addr += 0x1000) {
        uint32_t phys = pmm_alloc_frame();
        if (!phys) {
            serial_puts("[ELF] Out of memory for stack\n");
            kfree(proc);
            return NULL;
        }
        vmm_map_page(addr, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }
    proc->stack_top = USER_STACK_TOP - 16;
    
    serial_puts("[ELF] Loaded successfully\n");
    return proc;
}

user_process_t *elf_load_from_file(const char *path)
{
    vfs_node_t *node = vfs_finddir(vfs_get_root(), path);
    if (!node) {
        serial_puts("[ELF] File not found: ");
        serial_puts(path);
        serial_puts("\n");
        return NULL;
    }
    
    uint32_t size = node->length;
    if (size == 0) {
        serial_puts("[ELF] Empty file\n");
        return NULL;
    }
    
    uint8_t *buffer = (uint8_t *)kmalloc(size);
    if (!buffer) {
        serial_puts("[ELF] Out of memory\n");
        return NULL;
    }
    
    if (vfs_read(node, 0, size, buffer) != (int)size) {
        serial_puts("[ELF] Read failed\n");
        kfree(buffer);
        return NULL;
    }
    
    user_process_t *proc = elf_load_from_memory(buffer, size);
    
    kfree(buffer);
    return proc;
}

void elf_execute(user_process_t *proc)
{
    uint32_t pid = process_create(proc->name, 1);
    process_t *kernel_proc = NULL;
    if (pid > 0) {
        process_set_current(pid);
        kernel_proc = process_get(pid);
        serial_puts("[ELF] Created process with PID ");
        char pidbuf[12];
        int pi = 0;
        uint32_t pn = pid;
        if (pn == 0) pidbuf[pi++] = '0';
        else while (pn > 0) { pidbuf[pi++] = '0' + (pn % 10); pn /= 10; }
        pidbuf[pi] = '\0';
        for (int j = 0; j < pi / 2; j++) { char t = pidbuf[j]; pidbuf[j] = pidbuf[pi-1-j]; pidbuf[pi-1-j] = t; }
        serial_puts(pidbuf);
        serial_puts("\n");
    }
    
    serial_puts("[ELF] Executing in Ring 3 at 0x");
    char hex[9];
    for (int k = 7; k >= 0; k--) {
        int nibble = (proc->entry >> (k * 4)) & 0xF;
        hex[7-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    hex[8] = '\0';
    serial_puts(hex);
    serial_puts("\n");
    
    /* Set up kernel stack for this process in TSS
     * When returning from UM, CPU will use this stack */
    extern void gdt_set_kernel_stack(uint32_t stack);
    
    uint32_t *kstack_base = (uint32_t *)kmalloc(4096);
    uint32_t kernel_stack = (uint32_t)kstack_base + 4096;
    
    if (kernel_proc) {
        kernel_proc->kernel_stack = (uint32_t)kstack_base;
        kernel_proc->elf_proc = proc;
    }
    
    serial_puts("[ELF] Kernel stack for TSS: 0x");
    char hex3[9];
    for (int k = 7; k >= 0; k--) {
        int nibble = (kernel_stack >> (k * 4)) & 0xF;
        hex3[7-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    hex3[8] = '\0';
    serial_puts(hex3);
    serial_puts("\n");
    
    gdt_set_kernel_stack(kernel_stack);
    
    /* Call constructors from kernel mode before jumping to UM
     * The .ctors section is already mapped in user space, we can read it */
    if (proc->init_array != 0 && proc->init_array_size > 0) {
        serial_puts("[ELF] Calling constructors from .ctors...\n");
        uint32_t num_ctors = proc->init_array_size / 4;
        uint32_t *ctor_table = (uint32_t *)proc->init_array;
        
        for (uint32_t i = 0; i < num_ctors; i++) {
            uint32_t ctor_addr = ctor_table[i];
            if (ctor_addr != 0 && ctor_addr != 0xFFFFFFFF) {
                serial_puts("[ELF] Calling ctor at 0x");
                char h[9];
                for (int k = 7; k >= 0; k--) {
                    int nibble = (ctor_addr >> (k * 4)) & 0xF;
                    h[7-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
                }
                h[8] = '\0';
                serial_puts(h);
                serial_puts("\n");
                
                typedef void (*ctor_fn_t)(void);
                ctor_fn_t fn = (ctor_fn_t)ctor_addr;
                fn();
            }
        }
        serial_puts("[ELF] Constructors complete\n");
    }
    
    uint32_t user_esp = proc->stack_top;
    
    /* Jump to Ring 3 using IRET
     * Stack layout for IRET: SS, ESP, EFLAGS, CS, EIP */
    uint32_t user_eip = proc->entry;
    
    serial_puts("[ELF] User ESP: 0x");
    char hex2[9];
    for (int k = 7; k >= 0; k--) {
        int nibble = (user_esp >> (k * 4)) & 0xF;
        hex2[7-k] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
    }
    hex2[8] = '\0';
    serial_puts(hex2);
    serial_puts("\n");
    
    serial_puts("[ELF] Jumping to user mode...\n");
    
    __asm__ volatile("cli");
    
    __asm__ volatile (
        "movw $0x43, %%ax\n"    /* User data segment: 0x40 | RPL 3 */
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "pushl $0x43\n"         /* SS - user data segment */
        "pushl %0\n"            /* ESP */
        "pushl $0x202\n"        /* EFLAGS - IF=1, reserved=1 */
        "pushl $0x3B\n"         /* CS - user code segment: 0x38 | RPL 3 */
        "pushl %1\n"            /* EIP */
        "iret\n"                /* Jump to Ring 3 */
        :
        : "r"(user_esp), "r"(user_eip)
        : "memory", "eax"
    );
    
    /* Should never reach here */
    serial_puts("[ELF] ERROR: Returned from user mode!\n");
}

void elf_free_process(user_process_t *proc)
{
    if (!proc) return;
    
    if (proc->page_dir) {
        uint32_t *pd = (uint32_t *)proc->page_dir;
        
        for (int i = 0; i < 768; i++) {
            if (pd[i] & PAGE_PRESENT) {
                uint32_t *pt = (uint32_t *)((pd[i] & 0xFFFFF000) + KERNEL_VMA);
                
                for (int j = 0; j < 1024; j++) {
                    if (pt[j] & PAGE_PRESENT) {
                        pmm_free_frame(pt[j] & 0xFFFFF000);
                    }
                }
                
                pmm_free_frame(pd[i] & 0xFFFFF000);
            }
        }
        
        pmm_free_frame(proc->page_dir - KERNEL_VMA);
    }
    
    kfree(proc);
}
