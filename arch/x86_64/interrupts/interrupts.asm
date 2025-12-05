extern divide_error_handler
extern invalid_opcode_handler
extern machine_check_handler
extern double_fault_handler
extern segment_not_present_handler
extern stack_fault_handler
extern gp_fault_handler
extern page_fault_handler
extern keyboard_int_handler
extern mouse_int_handler
extern apic_timer_int_handler
extern spurious_int_handler
extern panic_ipi_handler

%macro ISR_NOERRCODE 1
global isr_%1
isr_%1:
    cli
    push qword 0        ; rsv
    push qword 0        ; Dummy for error code
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rdx
    push rcx
    push rbx
    push rax

    mov rdi, rsp        ; trap_frame* as 1. argument
    call %1_handler

    ; Restore registers
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    add rsp, 8          ; Skip error code
    add rsp, 8          ; rsv cleanup
    iretq
%endmacro

%macro ISR_ERRCODE 1
global isr_%1
isr_%1:
    cli
    push qword 0 ; rsv
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rdx
    push rcx
    push rbx
    push rax

    mov rdi, rsp        ; trap_frame* als 1. Argument
    call %1_handler

    ; Restore registers
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    add rsp, 8          ; Skip error code
    add rsp, 8          ; rsv cleanup
    iretq
%endmacro

; Exceptions without Error Code
ISR_NOERRCODE divide_error
ISR_NOERRCODE invalid_opcode
ISR_NOERRCODE machine_check

; Exceptions with Error Code
ISR_ERRCODE double_fault
ISR_ERRCODE segment_not_present
ISR_ERRCODE stack_fault
ISR_ERRCODE gp_fault
ISR_ERRCODE page_fault

; Device Interrupts
ISR_NOERRCODE keyboard_int
ISR_NOERRCODE mouse_int
ISR_NOERRCODE apic_timer_int
ISR_NOERRCODE spurious_int
ISR_NOERRCODE panic_ipi