global usermode_entry_trampoline

usermode_entry_trampoline:
    mov ax, 0x23              ; user data segment | 3
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Stack aufbauen wie iretq erwartet
    push 0x23                 ; SS
    push rdi                  ; RSP (user stack)
    pushfq
    push 0x1B                 ; CS (user code segment | 3)
    push rsi                  ; RIP (user entry point)
    iretq
