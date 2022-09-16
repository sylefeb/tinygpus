/* from https://raw.githubusercontent.com/YosysHQ/picorv32/master/picosoc/sections.lds */
/* modified for icev-conveyor */

MEMORY
{
    ROM (xrw)  : ORIGIN = 0x00000000, LENGTH = 0x08000  /* 32 KB */
    RAM (rx)   : ORIGIN = 0x00008000, LENGTH = 0x18000  /* 96 KB */
}

SECTIONS {
    /* The program code and other data goes into ROM, mapped in a first BRAM */
    .text :
    {
        . = ALIGN(4);
        *(.text)           /* .text sections (code) */
        *(.text*)          /* .text* sections (code) */
    } >ROM

    /* The initialized data goes into RAM, mapped in a second BRAM */
    .data :
    {
        . = ALIGN(4);
        *(.rodata)         /* .rodata sections (constants, strings, etc.) */
        *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
        *(.srodata)        /* .rodata sections (constants, strings, etc.) */
        *(.srodata*)       /* .rodata* sections (constants, strings, etc.) */
        *(.data)           /* .data sections */
        *(.data*)          /* .data* sections */
        *(.sdata)          /* .sdata sections */
        *(.sdata*)         /* .sdata* sections */
    } >RAM

    /* Uninitialized data goes into RAM, mapped in a second BRAM  */
    .bss :
    {
        . = ALIGN(4);
        *(.bss)
        *(.bss*)
        *(.sbss)
        *(.sbss*)
        *(COMMON)
    } >RAM

}
