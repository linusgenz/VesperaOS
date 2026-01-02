global setjmp
global longjmp

; int setjmp(jmp_buf env)
; env in RDI, return 0
setjmp:
    mov [rdi + 0], rsp
    mov [rdi + 8], rbp
    mov [rdi + 16], rbx
    mov [rdi + 24], r12
    mov [rdi + 32], r13
    mov [rdi + 40], r14
    mov [rdi + 48], r15
    xor eax, eax
    ret

; void longjmp(jmp_buf env, int val)
; env in RDI, val in ESI
longjmp:
    mov rsp, [rdi + 0]
    mov rbp, [rdi + 8]
    mov rbx, [rdi + 16]
    mov r12, [rdi + 24]
    mov r13, [rdi + 32]
    mov r14, [rdi + 40]
    mov r15, [rdi + 48]
    mov eax, esi
    test eax, eax
    jne .ok
    mov eax, 1
.ok:
    ret
