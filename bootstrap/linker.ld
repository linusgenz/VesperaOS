OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

SECTIONS
{
    . = 0x50000;
       _PhysStart = .;
    /* Text Segment */
    .text :
    {
        *(.text.entry)
        *(.text*)
    }

    /* Read-only Data Segment */
    .rodata :
    {
        *(.rodata*)
    }

    /* Data Segment */
    .data :
    {
        *(.data*)
    }

    /* BSS Segment (zero-init) */
    .bss (NOLOAD) :
    {
        *(.bss*)
        *(COMMON)
    }

    /* Align End */
    /DISCARD/ : { *(.eh_frame) }
    _PhysEnd = .;
}
