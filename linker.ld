OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kernel_main)

KERNEL_VMA  = 0xFFFFFFFF80000000;
KERNEL_LMA  = 0x00100000;

SECTIONS
{
    . = KERNEL_VMA;

    kernel_virt_start = .;

    .cpu_startup_reports : AT(KERNEL_LMA + (ADDR(.cpu_startup_reports) - KERNEL_VMA))
    {
        KEEP(*(.cpu_startup_reports))
    }
            /*.ap_trampoline 0x00008000 : ALIGN(0x1000)
            {
                KEEP(*(.text.ap_trampoline))
            }*/

    .text : AT(KERNEL_LMA + (ADDR(.text) - KERNEL_VMA))
    {
        *(.text*)
    }

    . = ALIGN(0x1000);

    .rodata : AT(KERNEL_LMA + (ADDR(.rodata) - KERNEL_VMA))
    {
        *(.rodata*)
    }

    . = ALIGN(0x1000);

    .data : AT(KERNEL_LMA + (ADDR(.data) - KERNEL_VMA))
    {
        *(.data*)
    }

    . = ALIGN(0x1000);

    .bss : AT(KERNEL_LMA + (ADDR(.bss) - KERNEL_VMA))
    {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }

    . = ALIGN(0x1000);

    kernel_virt_end = .;
    kernel_phys_start = KERNEL_LMA;
    kernel_phys_end = KERNEL_LMA + (kernel_virt_end - kernel_virt_start);
}