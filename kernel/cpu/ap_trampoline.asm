; AP Trampoline for transitioning from 16-bit to 64-bit mode
; Gets loaded at 0x7000 (physical memory)
[BITS 16]
[ORG 0x0000]

start:
    cli
    cld

    mov al, 'X'
    out 0xE9, al

    mov ax, 0x0000
    mov ds, ax
    lgdt [gdt_desc]

    ; Enable protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

        mov al, 'Y'
        out 0xE9, al


    jmp 0x08:protected_mode

; ---------------------
; Protected Mode (32-bit)
; ---------------------
[BITS 32]
protected_mode:
    mov al, 'P'
    out 0xE9, al

    ; Set up segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Load PML4 address
    mov eax, [pml4_ptr]
    mov cr3, eax

    mov al, 'Q'
    out 0xE9, al

    ; Enable PAE
    mov eax, cr4
    or eax, (1 << 5)        ; PAE bit
    mov cr4, eax

    ; Enable Long Mode in EFER
    mov ecx, 0xC0000080     ; IA32_EFER MSR
    rdmsr
    or eax, (1 << 8)        ; LME (Long Mode Enable)
    wrmsr

    mov al, 'R'
    out 0xE9, al

    ; Enable paging (this activates long mode)
    mov eax, cr0
    or eax, (1 << 31)       ; PG bit
    mov cr0, eax

    ; Serialize execution
    jmp flush_pipeline
flush_pipeline:

    mov al, 'S'
    out 0xE9, al

    ; Far jump to 64-bit code segment
    jmp 0x08:long_mode

; ---------------------
; Long Mode (64-bit)
; ---------------------
[BITS 64]
long_mode:
    mov al, 'L'
    out 0xE9, al

    ; Set up 64-bit segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Load stack pointer
    mov rsp, [stack_ptr]

    ; Align stack to 16 bytes (required for x86-64 ABI)
    and rsp, 0xFFFFFFFFFFFFFFF0

    mov al, 'M'
    out 0xE9, al

    ; Call your AP entry point via interrupt
    int 0x30

halt:
    hlt
    jmp halt

; ---------------------------------------
; GDT Structure
; ---------------------------------------


align 8
gdt_desc:
    dw gdt_end - gdt - 1
    dd 0x7210

; Pad to offset 0x200
times 0x200 - ($ - $$) db 0

pml4_ptr:   dq 0                ; BSP writes PML4 physical address here
stack_ptr:  dq 0                ; BSP writes stack physical address here
gdt:
    ; Null descriptor
    dq 0x0000000000000000

    ; 32-bit Code Segment for Protected Mode transition
    ; Base=0, Limit=0xFFFFF, Granularity=4KB, Size=32bit, Present=1, DPL=0
    dq 0x00CF9A000000FFFF

    ; 32-bit Data Segment
    ; Base=0, Limit=0xFFFFF, Granularity=4KB, Size=32bit, Present=1, DPL=0
    dq 0x00CF92000000FFFF
gdt_end:
