global _start
extern main
extern init_environ
extern stdin
extern stdout
extern stderr
extern sys_exit

%define HANDLE_TYPE_TTY 0x1000000000000000
%define HANDLE_STDIN        (HANDLE_TYPE_TTY | 0)
%define HANDLE_STDOUT       (HANDLE_TYPE_TTY | 1)
%define HANDLE_STDERR       (HANDLE_TYPE_TTY | 2)

section .text

_start:
    push rdi
    push rsi

    mov rdi, rdx            ; rdx = envp
    call init_environ

    pop rsi
    pop rdi

    mov  rcx, HANDLE_STDIN
    lea  rax, [rel stdin]
    mov  [rax], rcx

    mov  rcx, HANDLE_STDOUT
    lea  rax, [rel stdout]
    mov  [rax], rcx

    mov  rcx, HANDLE_STDERR
    lea  rax, [rel stderr]
    mov  [rax], rcx

    call main

    mov  rdi, rax            ; return code
    xor  rsi, rsi
    xor  rdx, rdx
    xor  rcx, rcx
    xor  r8,  r8
    xor  r9,  r9
    call sys_exit

    jmp $