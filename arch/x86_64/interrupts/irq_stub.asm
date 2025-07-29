; irq_stub.asm
;
; LuminOS - operating system for the x86_64 architecture
;
; Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
;
; Created by Linus Genz on 28.07.25.
;
; This file is part of LuminOS.
;
; LuminOS is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; LuminOS is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with LuminOS. If not, see <https:;www.gnu.org/licenses/>.

%macro IRQ_STUB 1
global irq_stub_%1
irq_stub_%1:
    push %1
    jmp irq_common_stub
%endmacro

IRQ_STUB 0x30    ; xhci int

global irq_common_stub

extern irq_common_stub_handler

irq_common_stub:
    pop rdi                  ; load irqno in rdi (1. arg)
    call irq_common_stub_handler
    iretq