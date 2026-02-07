#ifndef _KERNEL_ELF_H
#define _KERNEL_ELF_H

#include <stdint.h>

#define ELF_MAGIC 0x464C457F
#define ELFCLASS32  1
#define ELFCLASS64  2
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define ET_NONE   0
#define ET_REL    1
#define ET_EXEC   2
#define ET_DYN    3
#define ET_CORE   4

#define EM_386    3

#define EV_CURRENT 1

#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6

#define PF_X  (1 << 0)
#define PF_W  (1 << 1)
#define PF_R  (1 << 2)

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;        
    uint16_t e_machine;     
    uint32_t e_version;     
    uint32_t e_entry;       
    uint32_t e_phoff;       
    uint32_t e_shoff;       
    uint32_t e_flags;       
    uint16_t e_ehsize;      
    uint16_t e_phentsize;   
    uint16_t e_phnum;       
    uint16_t e_shentsize;   
    uint16_t e_shnum;       
    uint16_t e_shstrndx;    
} elf_header_t;

#define EI_MAG0     0   /* 0x7F */
#define EI_MAG1     1   /* 'E' */
#define EI_MAG2     2   /* 'L' */
#define EI_MAG3     3   /* 'F' */
#define EI_CLASS    4   
#define EI_DATA     5   
#define EI_VERSION  6   
#define EI_OSABI    7

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} elf_phdr_t;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} elf_shdr_t;

#define SHT_NULL          0
#define SHT_PROGBITS      1
#define SHT_SYMTAB        2
#define SHT_STRTAB        3
#define SHT_INIT_ARRAY    14
#define SHT_FINI_ARRAY    15
#define SHT_PREINIT_ARRAY 16

typedef struct {
    uint32_t pid;
    uint32_t entry;
    uint32_t stack_top;
    uint32_t page_dir;
    uint32_t init_array;
    uint32_t init_array_size;
    uint32_t fini_array;
    uint32_t fini_array_size;
    char name[32];
} user_process_t;

user_process_t *elf_load_from_file(const char *path);
user_process_t *elf_load_from_memory(const uint8_t *data, uint32_t size);

void elf_execute(user_process_t *proc);
void elf_free_process(user_process_t *proc);

#endif 
