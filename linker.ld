OUTPUT_FORMAT(elf64-x86-64)
ENTRY(limine_entry)

KERNEL_BASE = 0xFFFFFFFF80000000;
PHYS_BASE   = 0x100000;

PHDRS
{
    text    PT_LOAD FLAGS(5);   /* R+X */
    rodata  PT_LOAD FLAGS(4);   /* R   */
    data    PT_LOAD FLAGS(6);   /* R+W */
}

SECTIONS
{
    . = KERNEL_BASE + PHYS_BASE;

    .requests_start_marker : AT(PHYS_BASE) {
        KEEP(*(.requests_start_marker))
    } :text

    .requests : AT(PHYS_BASE + SIZEOF(.requests_start_marker)) {
        KEEP(*(.requests))
    } :text

    .requests_end_marker : AT(PHYS_BASE
                               + SIZEOF(.requests_start_marker)
                               + SIZEOF(.requests)) {
        KEEP(*(.requests_end_marker))
    } :text

    .text : ALIGN(0x1000) {
        kernel_start = .;
        *(.text._start)
        *(.text*)
    } :text

    .rodata : ALIGN(0x1000) {
        *(.rodata*)
    } :rodata

    .data.font : ALIGN(0x1000) {
        *(.data.font)
    } :rodata

    .data : ALIGN(0x1000) {
        *(.data*)
    } :data

    .bss : ALIGN(0x1000) {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
        kernel_end = .;
    } :data

    /DISCARD/ : {
        *(.eh_frame)
        *(.note*)
    }
}