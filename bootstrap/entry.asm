global _start
extern bootstrap_main

section .text
_start:
    cli
    mov rsp, bootstrap_stack_top
    call bootstrap_main
.hang:
    hlt
    jmp .hang

section .bss
align 16
bootstrap_stack:
    resb 16384
bootstrap_stack_top:
