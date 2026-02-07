/* ZurichOS Linker Script
 * Higher-half kernel at 0xC0000000
 * Loaded at physical address 0x00100000 (1 MB mark)
 */

ENTRY(_start)

/* Virtual address where kernel is linked */
KERNEL_VMA = 0xC0000000;

/* Physical address where kernel is loaded */
KERNEL_LMA = 0x00100000;

SECTIONS
{
    /* Start at 1 MB physical, linked at higher-half virtual */
    . = KERNEL_LMA;

    /* Multiboot header must be in first 8KB */
    .multiboot ALIGN(4K) :
    {
        *(.multiboot)
    }

    /* Bootstrap code runs before paging is enabled */
    .boot ALIGN(4K) :
    {
        *(.boot)
        *(.boot.*)
    }

    /* Now switch to higher-half virtual addresses */
    . += KERNEL_VMA;

    /* Kernel code section */
    _kernel_start = .;
    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VMA)
    {
        _text_start = .;
        *(.text)
        *(.text.*)
        _text_end = .;
    }

    /* Read-only data */
    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VMA)
    {
        _rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        _rodata_end = .;
    }

    /* Initialized data */
    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VMA)
    {
        _data_start = .;
        *(.data)
        *(.data.*)
        _data_end = .;
    }

    /* Uninitialized data (BSS) */
    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VMA)
    {
        _bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        _bss_end = .;
    }

    /* End of kernel */
    . = ALIGN(4K);
    _kernel_end = .;
    _kernel_end_phys = . - KERNEL_VMA;

    /* Calculate kernel size */
    _kernel_size = _kernel_end - KERNEL_VMA - KERNEL_LMA;

    /* Discard unwanted sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}
