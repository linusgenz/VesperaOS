; syscall_wrapper.asm

bits 64
global syscall_wrapper

section .text
syscall_wrapper:
    ; Args:
    ; RDI = num     → soll in RAX
    ; RSI = arg0    → RDI
    ; RDX = arg1    → RSI
    ; RCX = arg2    → RDX
    ; R8  = arg3    → R10
    ; R9  = arg4    → R8
    ; [RSP+8]       → arg5 → R9 (Stack, da RSP für return address steht)

    mov     rax, rdi      ; syscall number → rax
    mov     rdi, rsi      ; arg0 → rdi
    mov     rsi, rdx      ; arg1 → rsi
    mov     rdx, rcx      ; arg2 → rdx
    mov     r10, r8       ; arg3 → r10
    mov     r8,  r9       ; arg4 → r8
    mov     r9,  [rsp+8]  ; arg5 → r9 (direkt hinter return address)

    syscall

    ret
