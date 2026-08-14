; ld_start.asm
; VesperaOS - operating system for the x86_64 architecture
;
; Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
;
; Created by Linus Genz on 13.08.26.
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
; along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

section .text
global _start
extern ld_start_c

_start:
    mov rbp, rsp
    and rsp, -16

    mov rdi, rbp
    call ld_start_c

    mov rax, 60

    mov rdi, 127
    syscall
    hlt
