; Flat binary, pure usermode
; Assembled with: nasm -f bin -o test_userprog.bin test_userprog.asm

BITS 64

%define STDIN_FD 0
%define STDOUT_FD 1
%define MAX_INPUT 128

extern strncmp

section .bss
buffer: resb MAX_INPUT

section .data
msg: db "Welcome to Minimal Shell!", 10
msg_len: equ $ - msg

prefix: db ">", 0
prefix_len: equ $ - prefix

hello_str: db "hello"
reboot_str: db "reboot"

hello_world: db "Hello World!", 10
hello_world_len: equ $ - hello_world

unknown_str: db "Unknown command: "
unknown_str_len:  equ $ - unknown_str

section .text
global _start

; long write(char *buf, size_t len);
write:
    mov rdx, rsi    ; length
    mov rsi, rdi    ; pointer to string
    mov rax, 1      ; syscall: write
    mov rdi, STDOUT_FD

    syscall
    ret

; long read(char *buf, size_t len);
read:
    mov rdx, rsi    ; length
    mov rsi, rdi    ; pointer to buf
    mov rax, 0      ; syscall: read
    mov rdi, STDIN_FD

    syscall
    ret

_start:
    lea rdi, [rel msg]  ; pointer to string
    mov rsi, msg_len    ; length
    call write

    .loop_start:
        lea rdi, [rel prefix]  ; pointer to string
        mov rsi, prefix_len    ; length
        call write

        lea rdi, [rel buffer]
        mov rsi, MAX_INPUT
        call read

        push rax

        lea rdi, [rel buffer]
        lea rsi, [rel reboot_str]
        mov rdx, 5
        call strncmp

        cmp rax, 0
        jne .no_match

    .match_found:
       ; lea rdi, [rel hello_world]
       ; mov rsi, hello_world_len
       ; call write
       ; jmp .loop_start
        mov rax, 169
        mov rdi, 0xfee1dead
        mov rsi, 672274793
        mov rdx, 0
        syscall

    .no_match:
        lea rdi, [rel unknown_str]
        mov rsi, unknown_str_len
        call write

        lea rdi, [rel buffer]
        pop rsi            ; length of buffer (ret from read)
        call write

        jmp .loop_start

    ; exit(0)
    mov rax, 60         ; syscall: exit
    xor rdi, rdi        ; exit code 0
    syscall

hang:
    jmp hang            ; in case exit fails

