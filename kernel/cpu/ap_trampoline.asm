TRAMPOLINE_BASE equ 0x8000
IDTR_SRC        equ 0x1000   ; vom Kernel vorbereitet
PML4_SRC        equ 0x2000   ; vom Kernel vorbereitet
KERNEL_ENTRY    equ 0x3000
REPORT_BASE     equ 0x7000   ; CpuStartupReport array

bits 16
org TRAMPOLINE_BASE

ap_trampoline_entry:
    cli

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; GDT
    lgdt [gdt32.desc - TRAMPOLINE_BASE + TRAMPOLINE_BASE]

    ; Protected Mode
    mov eax, cr0
    or  eax, 1
    mov cr0, eax

    jmp 0x08:(ap32 - TRAMPOLINE_BASE + TRAMPOLINE_BASE)

bits 32
ap32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; PAE
    mov eax, cr4
    or  eax, (1 << 5)
    mov cr4, eax

    mov eax, [abs PML4_SRC]
    mov cr3, eax

    ; Long Mode
    mov ecx, 0xC0000080
    rdmsr
    or  eax, (1 << 8)
    wrmsr

    ; Paging + Protected Mode
    mov eax, cr0
    or  eax, (1 << 31) | 1
    mov cr0, eax

    ; GDT64
    lgdt [gdt64.desc - TRAMPOLINE_BASE + TRAMPOLINE_BASE]

    jmp 0x08:(ap64 - TRAMPOLINE_BASE + TRAMPOLINE_BASE)

bits 64
ap64:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    lidt [abs IDTR_SRC]

    mov eax, 1
    cpuid
    shr ebx, 24
    and ebx, 0xFF           ; APIC ID in EBX

    ; report address: 0x7000 + apic_id * sizeof(CpuStartupReport)
    ; sizeof = 24 Bytes
    imul rax, rbx, 24
    add rax, REPORT_BASE

    ; load stack from report
    mov rsp, [rax + 8]      ; stack_pointer offset = 8

    mov byte [rax + 16], 1  ; ready offset = 16

    ; wait till go = true
.wait:
    pause
    cmp byte [rax + 17], 1  ; go offset = 17
    jne .wait

    mov rax, [abs KERNEL_ENTRY]
    jmp rax

; ============================================================
; GDT
; ============================================================
align 8
gdt32:
    dq 0                    ; null
    dq 0x00CF9A000000FFFF   ; 32-bit code
    dq 0x00CF92000000FFFF   ; 32-bit data
.desc:
    dw (gdt32.desc - gdt32 - 1)
    dd (gdt32 - TRAMPOLINE_BASE + TRAMPOLINE_BASE)

align 8
gdt64:
    dq 0                    ; null
    dq 0x00AF9A000000FFFF   ; 64-bit code
    dq 0x00AF92000000FFFF   ; 64-bit data
.desc:
    dw (gdt64.desc - gdt64 - 1)
    dd (gdt64 - TRAMPOLINE_BASE + TRAMPOLINE_BASE)