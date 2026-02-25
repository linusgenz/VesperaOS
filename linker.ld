OUTPUT_FORMAT(elf64-x86-64)
ENTRY(limine_entry)

KERNEL_BASE = 0xFFFFFFFF80000000;
PHYS_BASE   = 0x100000;

SECTIONS
{

    .cpu_startup_reports 0x00007000 : AT(0x00007000) {
        KEEP(*(.cpu_startup_reports))
    }

    .ap_trampoline 0x00008000 : AT(0x00008000) {
        KEEP(*(.text.ap_trampoline))
    }


    . = KERNEL_BASE + PHYS_BASE;

    .requests_start_marker : AT(PHYS_BASE) {
        KEEP(*(.requests_start_marker))
    }

    .requests : AT(PHYS_BASE + SIZEOF(.requests_start_marker)) {
        KEEP(*(.requests))
    }

    .requests_end_marker : AT(PHYS_BASE
                               + SIZEOF(.requests_start_marker)
                               + SIZEOF(.requests)) {
        KEEP(*(.requests_end_marker))
    }

    /* ------------------------------------------------------------------ */
    /* Code                                                                 */
    /* ------------------------------------------------------------------ */
    .text : ALIGN(0x1000) {
        _KernelStart = .;
        *(.text._start)
        *(.text*)
    }

    /* ------------------------------------------------------------------ */
    /* Read-only Daten                                                      */
    /* ------------------------------------------------------------------ */
    .rodata : ALIGN(0x1000) {
        *(.rodata*)
    }

    /* ------------------------------------------------------------------ */
    /* Eingebettete Binärdaten (Font via objcopy)                           */
    /* ------------------------------------------------------------------ */
    .data.font : ALIGN(0x1000) {
        *(.data.font)
    }

    /* ------------------------------------------------------------------ */
    /* Schreibbare Daten                                                    */
    /* ------------------------------------------------------------------ */
    .data : ALIGN(0x1000) {
        *(.data*)
    }

    /* ------------------------------------------------------------------ */
    /* BSS – nicht initialisierte Daten                                     */
    /* ------------------------------------------------------------------ */
    .bss : ALIGN(0x1000) {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
        _KernelEnd = .;
    }

    /* ------------------------------------------------------------------ */
    /* Debug Informationen (nicht geladen)                                  */
    /* ------------------------------------------------------------------ */
    /DISCARD/ : {
        *(.eh_frame)
        *(.note*)
    }
}
