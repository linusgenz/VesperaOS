global syscall_entry
extern syscall_handler

section .text
bits 64

%define SAVED_USER_RSP 0x50
%define STACK_POINTER 0x18
%define KERNEL_RSP_AFTER_SYSCALL 0x68
%define FROM_SYSCALL_BOOL 0x98

syscall_entry:
    swapgs                        ; GS.base = Kernel GS

    cli

    mov r15, qword [gs:0]      ; current context pointer (execution_context_t*)

    mov [r15 + SAVED_USER_RSP], rsp

    mov byte [r15 + FROM_SYSCALL_BOOL], 1

    mov rsp, qword [r15 + STACK_POINTER]


    push r11                      ; Save user RFLAGS
    push rcx                      ; Save user RIP (sysret will pop into RCX)

    mov [r15 + KERNEL_RSP_AFTER_SYSCALL], rsp

    ; rax  = syscall number
    ; rdi  =  (num)
    ; rsi  = arg0
    ; rdx  = arg1
    ; rcx  = arg2
    ; r8   = arg3
    ; r9   = arg4
    ; arg 5 (stack)

    mov r11, rax
    mov rax, rdi
    mov rdi, r11


    mov r11, rsi
    mov rsi, rax
    mov rax, r11

    mov r11, rdx
    mov rdx, rax
    mov rax, r11

    mov rcx, rax

    mov r11, r8
    mov r8, r10
    mov rax, r11

    mov r11, r9
    mov r9, rax

    sub rsp, 8      ; arg5 has to go onto the stack (System V ABI)
    mov [rsp], r11

    call syscall_handler

    add rsp, 8       ; Stack cleanup

    pop rcx          ; user RIP -> RCX
    pop r11          ; user RFLAGS -> R11


    mov byte [r15 + FROM_SYSCALL_BOOL], 0
    mov rsp, qword [r15 + SAVED_USER_RSP]

    or r11, 0x200

    swapgs

    o64 sysret