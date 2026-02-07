bits 32

MULTIBOOT_MAGIC     equ 0x1BADB002
MULTIBOOT_ALIGN     equ 1 << 0          
MULTIBOOT_MEMINFO   equ 1 << 1          
MULTIBOOT_FLAGS     equ MULTIBOOT_ALIGN | MULTIBOOT_MEMINFO
MULTIBOOT_CHECKSUM  equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)


KERNEL_VMA          equ 0xC0000000
KERNEL_PAGE_INDEX   equ (KERNEL_VMA >> 22)

global boot_page_directory

section .multiboot
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM

; Bootstrap Stack used before paging

section .boot.bss nobits alloc noexec write
align 16
boot_stack_bottom:
    resb 16384              
boot_stack_top:

; Bootstrap Page Tables

section .boot.bss nobits alloc noexec write
align 4096

boot_page_directory:
    resb 4096

boot_page_table_identity:
    resb 4096

; Page table for higher-half mapping
boot_page_table_kernel:
    resb 4096

; Bootstrap Code runs with paging disabled

section .boot
global _start

_start:
    cli

    mov [multiboot_info_ptr], ebx
    mov [multiboot_magic], eax
    mov esp, boot_stack_top
    mov ecx, 1024
    xor eax, eax
    mov edi, boot_page_directory
    rep stosd
    mov edi, boot_page_table_identity
    mov eax, 0x00000003
    mov ecx, 1024

.fill_identity:
    mov [edi], eax
    add eax, 4096           
    add edi, 4
    loop .fill_identity
    mov edi, boot_page_table_kernel
    mov eax, 0x00000003    
    mov ecx, 1024

.fill_kernel:
    mov [edi], eax
    add eax, 4096
    add edi, 4
    loop .fill_kernel

    ; Entry 0: Identity map (first 4 MB)
    mov eax, boot_page_table_identity
    or eax, 0x03            ; Present + RW
    mov [boot_page_directory + 0*4], eax

    ; Entry 768 (0xC0000000 >> 22 = 768): Higher-half kernel
    mov eax, boot_page_table_kernel
    or eax, 0x03            ; Present + RW
    mov [boot_page_directory + KERNEL_PAGE_INDEX*4], eax
    mov eax, boot_page_directory
    mov cr3, eax
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    mov eax, higher_half
    jmp eax


section .text
higher_half:
    mov dword [boot_page_directory + KERNEL_VMA], 0
    mov eax, cr3
    mov cr3, eax
    mov esp, kernel_stack_top
    push dword [multiboot_magic + KERNEL_VMA]      
    push dword [multiboot_info_ptr + KERNEL_VMA]

    extern kernel_main
    call kernel_main

.halt:
    cli
    hlt
    jmp .halt

; Kernel Stack

section .bss
align 16
kernel_stack_bottom:
    resb 65536              ; 64 KB kernel stack
global kernel_stack_top
kernel_stack_top:

section .boot
multiboot_info_ptr:
    dd 0
multiboot_magic:
    dd 0
