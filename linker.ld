OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kernel_main)

SECTIONS
{
    g_localApicAddr = 0x00006000;
    .cpu_startup_reports 0x00007000 :
    {
        KEEP(*(.cpu_startup_reports))
    }
    .ap_trampoline 0x00008000 : ALIGN(0x1000)
    {
        KEEP(*(.text.ap_trampoline))
    }
    . = 0x100000;
	_KernelStart = .;
	.text : ALIGN(0x1000)
	{
		*(.text)
	}
	.data : ALIGN(0x1000)
	{
		*(.data)
	}
	.rodata : ALIGN(0x1000)
	{
		*(.rodata)
	}
	.bss : ALIGN(0x1000)
	{
        __bss_start = .;
        *(COMMON)
        *(.bss)
        __bss_end = .;
	}
	_KernelEnd = .;
}