global context_switch
section .text

; void context_switch(kthread_t* current, kthread_t* next)
; rdi = current
; rsi = next

context_switch:
    ; if (current != nullptr)
    test rdi, rdi
    jz .load_next_context

    mov [rdi + 0x18], rsp
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov [rdi + 0x18], rsp

.load_next_context:

    mov rsp, [rsi + 0x18]

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ret

