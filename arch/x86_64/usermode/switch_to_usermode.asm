global switch_to_user_mode
extern usermode_write_test
section .text
bits 64

; void switch_to_user_mode(void* user_stack_top)
switch_to_user_mode:
    ;  RDI = Usermode Stackpointer (top)
    ;  RSI = Usermode code

	mov ax, (4 * 8) | 3 ; ring 3 data with bottom 2 bits set for ring 3
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax ; SS is handled by iret

	; set up the stack frame iret expects
	push (4 * 8) | 3 ; data selector
	push rdi
	pushfq ; eflags
	push (3 * 8) | 3 ; code selector (ring 3 code with bottom 2 bits set for ring 3)
	push rsi ; instruction address to return to
	iretq
