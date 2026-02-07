/* Linker script for user programs */

ENTRY(_start)

SECTIONS
{
    . = 0x08048000;
    
    .text :
    {
        *(.text)
        *(.rodata)
    }
    
    .data :
    {
        *(.data)
    }
    
    .bss :
    {
        *(.bss)
        *(COMMON)
    }
}
