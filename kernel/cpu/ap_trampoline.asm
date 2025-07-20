section .text.ap_trampoline
global ap_trampoline_entry

%define kernel_stacks               0x00020000
%define idt                         0x1000
%define vm_pml4                     0x2000

%define local_apic_address          0x6000
%define cpu_startup_report_addr     0x7000
%define loader_base                 0x8000
%define boot_sector_base            0x7c00
%define IRQ_AP_ENTRY                0x30

[BITS 16]

ap_trampoline_entry:
        lgdt [gdt32.desc]

        mov eax, cr0
        or al, 0x01
        mov cr0, eax

        jmp gdt32.code:ap32         ; Jump to 32-bit code

; 64-bit GDT
gdt64:
        dq 0x0000000000000000       ; Null Descriptor
.code equ $ - gdt64                 ; Code segment
        dq 0x0020980000000000
.data equ $ - gdt64                 ; Data segment
        dq 0x0000920000000000

.desc:
        dw $ - gdt64 - 1            ; 16-bit Size (Limit)
        dq gdt64                    ; 64-bit Base Address

; -------------------------------------------------------------------------------------------------
; 32-bit GDT
gdt32:
        dq 0x0000000000000000       ; Null Descriptor
.code equ $ - gdt32                 ; Code segment
        dq 0x00cf9a000000ffff
.data equ $ - gdt32                 ; Data segment
        dq 0x00cf92000000ffff

.desc:
        dw $ - gdt32 - 1            ; 16-bit Size (Limit)
        dd gdt32                    ; 32-bit Base Address


[BITS 32]
ap32:
        mov eax, gdt32.data
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax
        mov esp, boot_sector_base

        lgdt [gdt64.desc]

     ;   mov eax, 0x000000a0         ; Set PAE and PGE
     ;   mov cr4, eax

        mov eax, cr4
        or eax, (1 << 5)            ; PAE
        or eax, (1 << 7)            ; PGE
        mov cr4, eax

        mov eax, [vm_pml4]            ; Assign PML4
        mov cr3, eax

        mov ecx, 0xc0000080         ; Read from EFER MSR
        rdmsr

        or eax, 0x00000100          ; Set LME
        wrmsr

        mov eax, cr0                ; Activate paging
        or eax, 0x80000000
        mov cr0, eax

        jmp gdt64.code:ap64         ; Jump to 64-bit code


[BITS 64]
ap64:

        xor rax, rax
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax

        ; Get next kernel stack
        mov rsp, 0x1000
        lock xadd [next_sp], rsp

        lidt [idt]

      ;  mov rsi, [local_apic_address]
     ;   add rsi, 0x00f0             ; Spurious Interrupt Vector
      ;  mov rdi, rsi
      ;  lodsd
      ;  or eax, 0x100
      ;  stosd

        sti

        mov eax, 1
        cpuid
        shr ebx, 24                 ; Initial APIC ID in bits 31:24
        mov ecx, ebx

        ; calc address for CpuStartupReport for the core
        mov rdi, 0x7000             ; rdi = base address of cpu_startup_reports
        mov rax, 24                 ; sizeof(CpuStartupReport) = 24 bytes
        mul ecx                     ; rax = offset for apic_id
        add rdi, rax                ; rdi = &cpu_startup_reports[apic_id]

        mov dword [rdi], ecx        ; apic_id (offset 0)
        mov [rdi + 8], rsp          ; stack_pointer (offset 4)
        mov byte [rdi + 16], 1      ; ready (offset 12)

        extern ap_main
        mov edi, ecx          ; ap_main(uint32_t apic_id)
        call ap_main

        hlt
        jmp $ ; idle

next_sp:    dq kernel_stacks
