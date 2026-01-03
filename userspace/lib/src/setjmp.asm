global setjmp
global longjmp

; int setjmp(jmp_buf env)
setjmp:
    ; Save callee-saved registers
    mov [rdi + 0],  rbx        ; Save RBX
    mov [rdi + 8],  rbp        ; Save RBP
    mov [rdi + 16], r12        ; Save R12
    mov [rdi + 24], r13        ; Save R13
    mov [rdi + 32], r14        ; Save R14
    mov [rdi + 40], r15        ; Save R15

    ; Save stack pointer (RSP)
    ; We save the RSP as it is before the call to setjmp
    lea rax, [rsp + 8]         ; Skip return address
    mov [rdi + 48], rax        ; Save RSP

    ; Save return address (RIP)
    mov rax, [rsp]             ; Get return address from stack
    mov [rdi + 56], rax        ; Save RIP

    ; Return 0 for direct call
    xor eax, eax
    ret

; void longjmp(jmp_buf env, int val)
longjmp:
    ; Restore callee-saved registers
    mov rbx, [rdi + 0]         ; Restore RBX
    mov rbp, [rdi + 8]         ; Restore RBP
    mov r12, [rdi + 16]        ; Restore R12
    mov r13, [rdi + 24]        ; Restore R13
    mov r14, [rdi + 32]        ; Restore R14
    mov r15, [rdi + 40]        ; Restore R15

    ; Restore stack pointer
    mov rsp, [rdi + 48]        ; Restore RSP

    ; Prepare return value
    ; longjmp should never return 0, if val is 0, return 1
    mov eax, esi               ; Move val to return register
    test eax, eax              ; Check if val is 0
    jnz .return                ; If not 0, use it
    inc eax                    ; If 0, make it 1

.return:
    ; Jump to saved return address
    jmp qword [rdi + 56]       ; Jump to saved RIP