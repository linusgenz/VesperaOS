; irq_stub.asm
;
; VesperaOS - operating system for the x86_64 architecture
;
; Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
;
; Created by Linus Genz on 28.07.25.
;
; This file is part of VesperaOS.
;
; VesperaOS is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; VesperaOS is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with VesperaOS. If not, see <https:;www.gnu.org/licenses/>.

%macro IRQ_STUB 1
global irq_stub_%1
irq_stub_%1:
    push %1
    jmp irq_common_stub
%endmacro

%assign i 0
%rep 256
    IRQ_STUB i
%assign i i+1
%endrep

global irq_common_stub
extern irq_common_stub_handler

irq_common_stub:
    pop rdi              ; irqno → 1. arg
    call irq_common_stub_handler
    iretq

section .data
global irq_stub_table
irq_stub_table:
%assign i 0
%rep 256
    dq irq_stub_%+i
%assign i i+1
%endrep