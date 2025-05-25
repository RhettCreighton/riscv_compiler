/* Simple linker script for RISC-V test programs */

MEMORY
{
    /* Code starts at 0x0 */
    TEXT (rx) : ORIGIN = 0x00000000, LENGTH = 16K
    
    /* Data starts at 0x4000 */
    DATA (rw) : ORIGIN = 0x00004000, LENGTH = 16K
}

SECTIONS
{
    . = 0x0;
    
    .text : {
        *(.text)
        *(.text.*)
    } > TEXT
    
    . = 0x4000;
    
    .data : {
        *(.data)
        *(.data.*)
    } > DATA
    
    .bss : {
        *(.bss)
        *(.bss.*)
    } > DATA
}

ENTRY(_start)