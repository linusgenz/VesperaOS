global syscall_entry
extern syscall_handler

section .text
bits 64

%define SAVED_USER_RSP 0x38
%define STACK_POINTER 0x18
%define TRAP_FRAME 152
syscall_entry:
    swapgs
    cli

    push r15

    mov r15, qword [gs:0]
    mov [r15 + SAVED_USER_RSP], rsp

    mov rsp, qword [r15 + STACK_POINTER]

    mov qword [r15 + TRAP_FRAME + 0x00], rax       ; rax (syscall number)
    mov qword [r15 + TRAP_FRAME + 0x08], rbx
    mov qword [r15 + TRAP_FRAME + 0x10], rcx       ; user RIP (gesichert von syscall)
    mov qword [r15 + TRAP_FRAME + 0x18], rdx
    mov qword [r15 + TRAP_FRAME + 0x20], rbp
    mov qword [r15 + TRAP_FRAME + 0x28], rsi
    mov qword [r15 + TRAP_FRAME + 0x30], rdi
    mov qword [r15 + TRAP_FRAME + 0x38], r8
    mov qword [r15 + TRAP_FRAME + 0x40], r9
    mov qword [r15 + TRAP_FRAME + 0x48], r10
    mov qword [r15 + TRAP_FRAME + 0x50], r11       ; user RFLAGS (gesichert von syscall)
    mov qword [r15 + TRAP_FRAME + 0x58], r12
    mov qword [r15 + TRAP_FRAME + 0x60], r13
    mov qword [r15 + TRAP_FRAME + 0x68], r14

    mov qword [r15 + TRAP_FRAME + 0x78], 0         ; rsv
    mov qword [r15 + TRAP_FRAME + 0x80], 0         ; error_code (kein Fehlercode bei syscall)

    mov qword [r15 + TRAP_FRAME + 0x88], rcx       ; rip = user RIP
    mov qword [r15 + TRAP_FRAME + 0x90], 0x23      ; cs = user code segment
    mov qword [r15 + TRAP_FRAME + 0x98], r11       ; rflags = user RFLAGS

    mov r14, [r15 + SAVED_USER_RSP]
    mov qword [r15 + TRAP_FRAME + 0xA0], r14       ; rsp = user RSP
    mov qword [r15 + TRAP_FRAME + 0xA8], 0x1b      ; ss = user data segment

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

    ; arg5 auf Stack
    sub rsp, 8
    push rax

    call syscall_handler

    add rsp, 16                    ; remove arg5

    mov [r15 + STACK_POINTER], rsp

    ; rax is already getting set in syscall_handler
    mov rbx, qword [r15 + TRAP_FRAME + 0x08]
    mov rdx, qword [r15 + TRAP_FRAME + 0x18]
    mov rbp, qword [r15 + TRAP_FRAME + 0x20]
    mov rsi, qword [r15 + TRAP_FRAME + 0x28]
    mov rdi, qword [r15 + TRAP_FRAME + 0x30]
    mov r8,  qword [r15 + TRAP_FRAME + 0x38]
    mov r9,  qword [r15 + TRAP_FRAME + 0x40]
    mov r10, qword [r15 + TRAP_FRAME + 0x48]
    mov r12, qword [r15 + TRAP_FRAME + 0x58]
    mov r13, qword [r15 + TRAP_FRAME + 0x60]
    mov r14, qword [r15 + TRAP_FRAME + 0x68]

    mov rcx, qword [r15 + TRAP_FRAME + 0x88]  ; user RIP
    mov r11, qword [r15 + TRAP_FRAME + 0x98]  ; user RFLAGS
    mov rsp, qword [r15 + TRAP_FRAME + 0xA0]  ; user RSP

    or r11, 0x200

    pop r15

    swapgs
    o64 sysret