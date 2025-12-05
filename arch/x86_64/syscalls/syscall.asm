global syscall_entry
extern syscall_handler

section .text
bits 64

%define SAVED_USER_RSP 0x50
%define STACK_POINTER 0x18
%define KERNEL_RSP_AFTER_SYSCALL 0x68
%define FROM_SYSCALL_BOOL 0x98

syscall_entry:
    swapgs
    cli

    mov r15, qword [gs:0]
    mov [r15 + SAVED_USER_RSP], rsp
    mov byte [r15 + FROM_SYSCALL_BOOL], 1
    mov rsp, qword [r15 + STACK_POINTER]

    push r11                      ; Save user RFLAGS
    push rcx                      ; Save user RIP

    mov [r15 + KERNEL_RSP_AFTER_SYSCALL], rsp

    ; Argumente umordnen
    xchg rax, rdi
    xchg rax, rsi
    xchg rax, rdx
    xchg rax, rcx
    xchg rax, r8
    xchg rax, r9

    ; arg5 auf Stack
    sub rsp, 8
    push rax

    call syscall_handler

    add rsp, 16                    ; Entferne arg5

    pop rcx                       ; user RIP -> RCX
    pop r11                       ; user RFLAGS -> R11

    mov [r15 + STACK_POINTER], rsp

    mov byte [r15 + FROM_SYSCALL_BOOL], 0
    mov rsp, qword [r15 + SAVED_USER_RSP]

    or r11, 0x200

    swapgs
    o64 sysret