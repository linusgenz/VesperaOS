global _start
extern main
extern init_environ
extern __stdio_init
extern exit

%define HANDLE_TYPE_DEVICE 0x7000000000000000
%define HANDLE_STDIN        (HANDLE_TYPE_DEVICE | 0)
%define HANDLE_STDOUT       (HANDLE_TYPE_DEVICE | 1)
%define HANDLE_STDERR       (HANDLE_TYPE_DEVICE | 2)

section .text

_start:
    push rdi
    push rsi

    mov rdi, rdx            ; rdx = envp
    call init_environ

    mov rdi, HANDLE_STDIN
    mov rsi, HANDLE_STDOUT
    mov rdx, HANDLE_STDERR
    call __stdio_init

    pop rsi
    pop rdi

    call main

    mov  rdi, rax            ; return code
    call exit

    jmp $