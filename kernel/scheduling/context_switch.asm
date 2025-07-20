; Function: context_switch(void **old_sp, void *new_sp)
; Saves current CPU state to old_sp and restores state from new_sp
; 
; Parameters:
;   RDI: void *old_sp - pointer to save current stack pointer (can be NULL)
;   RSI: void *new_sp  - new stack pointer to restore from
;
; Stack frame layout (pushed in this order, 64 bytes total):
; +56: RFLAGS
; +48: R15
; +40: R14  
; +32: R13
; +24: R12
; +16: RBX
; +8:  RBP
; +0:  Return address (RIP)

[BITS 64]
section .text

global context_switch
global context_switch_fast

context_switch:
    ; Disable interrupts during context switch

 ;   pushfq                  ; Save current RFLAGS
    cli                     ; Disable interrupts
    
    ; Check if we need to save current context
    test    rdi, rdi
    jz      .restore_only

    lea     rax, [rel .resume_context]

    push rax        ;
    pushfq
    push r15
    push r14
    push r13
    push r12
    push rbx
    push rbp        ;


    mov     [rdi], rsp

    .restore_only:
    ; Restore new context
    mov     rsp, rsi

    pop     rbp
    pop     rbx
    pop     r12
    pop     r13
    pop     r14
    pop     r15
    popfq
    ret


.resume_context:
    ; Ausführung geht hier weiter, wenn dieser Thread wieder eingeplant wird
    sti
    ret

; Alternative faster version for specific use cases
context_switch_fast:
    ; Fast context switch without interrupt disable
    ; Use only when you're sure interrupts are already disabled
    
    test    rdi, rdi
    jz      .fast_restore_only
    
    ; Save minimal context (no RFLAGS)
    push    rbp
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    
    mov     [rdi], rsp
    
.fast_restore_only:
    mov     rsp, rsi
    
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp
    ret

