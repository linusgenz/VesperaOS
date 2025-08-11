global switch_to_user_mode
section .text
bits 64

; void switch_to_user_mode(void* user_stack_top)
switch_to_user_mode:
    ;  RDI = Usermode Stackpointer (top)
    ;  RSI = Usermode code

	mov ax, 0x23 ; ring 3 data with bottom 2 bits set for ring 3
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax ; SS is handled by iret

	push 0x23 ; data selector
	push rdi
;	pushfq ; eflags
    push 0x202
	push 0x1B ; code selector (ring 3 code with bottom 2 bits set for ring 3)
	push rsi

	iretq
