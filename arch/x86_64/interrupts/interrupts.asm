; interrupts.asm — VesperaOS x86_64 unified ISR stubs

%define TF_SIZE   0xB0

%define TF_R15        0x00
%define TF_R14        0x08
%define TF_R13        0x10
%define TF_R12        0x18
%define TF_R11        0x20
%define TF_R10        0x28
%define TF_R9         0x30
%define TF_R8         0x38
%define TF_RBP        0x40
%define TF_RDI        0x48
%define TF_RSI        0x50
%define TF_RDX        0x58
%define TF_RCX        0x60
%define TF_RBX        0x68
%define TF_RAX        0x70
%define TF_VECTOR     0x78
%define TF_ERROR      0x80
%define TF_RIP        0x88
%define TF_CS         0x90
%define TF_RFLAGS     0x98
%define TF_RSP        0xA0
%define TF_SS         0xA8

%define HDR_VECTOR    0x00
%define HDR_ERROR     0x08
%define HDR_RIP       0x10
%define HDR_CS        0x18
%define HDR_RFLAGS    0x20
%define HDR_RSP       0x28
%define HDR_SS        0x30

bits 64
section .text

%macro ISR_STUB 1
global isr_stub_%1
isr_stub_%1:
%if (%1 == 8) || (%1 == 10) || (%1 == 11) || (%1 == 12) || \
    (%1 == 13) || (%1 == 14) || (%1 == 17) || (%1 == 21) || \
    (%1 == 29) || (%1 == 30)
    ; CPU already pushed error_code, just push vector
    push    qword %1
%else
    ; No hardware error code, push 0 then vector
    push    qword 0
    push    qword %1
%endif
    jmp     isr_common_entry
%endmacro

%assign i 0
%rep 256
    ISR_STUB i
%assign i i+1
%endrep

section .data
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 256
    dq isr_stub_%+i
%assign i i+1
%endrep

section .text

; Common entry - called from every per-vector stub
;
; On entry RSP points at:
;   [RSP+0]   vector  (u64)
;   [RSP+8]   error_code  (u64)
;   [RSP+16]  rip           CPU iret frame
;   [RSP+24]  cs
;   [RSP+32]  rflags
;   [RSP+40]  rsp
;   [RSP+48]  ss

extern vespera_trap_handler

global isr_common_entry
isr_common_entry:
    cld
    sub     rsp, TF_SIZE

    mov     [rsp + TF_R15], r15
    mov     [rsp + TF_R14], r14
    mov     [rsp + TF_R13], r13
    mov     [rsp + TF_R12], r12
    mov     [rsp + TF_R11], r11
    mov     [rsp + TF_R10], r10
    mov     [rsp + TF_R9],  r9
    mov     [rsp + TF_R8],  r8
    mov     [rsp + TF_RBP], rbp
    mov     [rsp + TF_RDI], rdi
    mov     [rsp + TF_RSI], rsi
    mov     [rsp + TF_RDX], rdx
    mov     [rsp + TF_RCX], rcx
    mov     [rsp + TF_RBX], rbx
    mov     [rsp + TF_RAX], rax

    ; Copy the header (vector, error_code, iret frame) into TrapFrame
    lea     rsi, [rsp + TF_SIZE]
    mov     rax, [rsi + HDR_VECTOR]
    mov     [rsp + TF_VECTOR], rax
    mov     rax, [rsi + HDR_ERROR]
    mov     [rsp + TF_ERROR], rax
    mov     rax, [rsi + HDR_RIP]
    mov     [rsp + TF_RIP], rax
    mov     rax, [rsi + HDR_CS]
    mov     [rsp + TF_CS], rax
    mov     rax, [rsi + HDR_RFLAGS]
    mov     [rsp + TF_RFLAGS], rax
    mov     rax, [rsi + HDR_RSP]
    mov     [rsp + TF_RSP], rax
    mov     rax, [rsi + HDR_SS]
    mov     [rsp + TF_SS], rax

    ; Swap GS if coming from user mode
    test    byte [rsp + TF_CS], 3
    jz      .from_kernel
    swapgs
    lfence
.from_kernel:

    mov     rdi, rsp
    push    rdi
    and     rsp, -16
    call    vespera_trap_handler
    pop     rsp

    lea     rsi, [rsp + TF_SIZE]
    mov     rax, [rsp + TF_RIP]
    mov     [rsi + HDR_RIP],    rax
    mov     rax, [rsp + TF_CS]
    mov     [rsi + HDR_CS],     rax
    mov     rax, [rsp + TF_RFLAGS]
    mov     [rsi + HDR_RFLAGS], rax
    mov     rax, [rsp + TF_RSP]
    mov     [rsi + HDR_RSP],    rax
    mov     rax, [rsp + TF_SS]
    mov     [rsi + HDR_SS],     rax

    mov     r15, [rsp + TF_R15]
    mov     r14, [rsp + TF_R14]
    mov     r13, [rsp + TF_R13]
    mov     r12, [rsp + TF_R12]
    mov     r11, [rsp + TF_R11]
    mov     r10, [rsp + TF_R10]
    mov     r9,  [rsp + TF_R9]
    mov     r8,  [rsp + TF_R8]
    mov     rbp, [rsp + TF_RBP]
    mov     rdi, [rsp + TF_RDI]
    mov     rsi, [rsp + TF_RSI]
    mov     rdx, [rsp + TF_RDX]
    mov     rcx, [rsp + TF_RCX]
    mov     rbx, [rsp + TF_RBX]
    mov     rax, [rsp + TF_RAX]

    add     rsp, TF_SIZE

    ; swapgs back if returning to user
    test    byte [rsp + HDR_CS], 3
    jz      .no_swapgs_back
    swapgs
    lfence
.no_swapgs_back:
    add     rsp, 16 ; skip vector & error code
    iretq