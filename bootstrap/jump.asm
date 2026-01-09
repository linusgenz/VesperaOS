global enter_higher_half

%define KERNEL_VIRT_BASE 0xFFFFFFFF80000000

section .text

enter_higher_half:
    mov     cr3, rdi

    lea     rax, [rel higher_half_entry]
    add     rax, qword KERNEL_VIRT_BASE
    jmp     rax

higher_half_entry:
    mov     rsp, rsi              ; RSI = kernel_stack_top
    xor     rbp, rbp

    and     rsp, -16
    sub     rsp, 8

    mov     rdi, rcx              ; boot_info → RCX
    call    rdx                   ; RDX = kernel_entry

.hang:
    hlt
    jmp     .hang