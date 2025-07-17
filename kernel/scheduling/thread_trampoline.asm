global thread_trampoline

section .text
thread_trampoline:
hlt
    pop rdi          ; Argument in rdi
    pop rax          ; Funktionspointer in rax
    call rax         ; Call: func(arg)

.hang:
    cli
    hlt
    jmp .hang
