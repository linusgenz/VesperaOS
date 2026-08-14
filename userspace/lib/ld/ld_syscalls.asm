; ld_syscalls.asm
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

global ld_raw_open
global ld_raw_close
global ld_raw_read
global ld_raw_seek
global ld_raw_exit
global ld_raw_write

%define SYS_OPEN  2
%define SYS_CLOSE 3
%define SYS_READ  0
%define SYS_WRITE 1
%define SYS_LSEEK 8
%define SYS_MMAP 9
%define SYS_MPROTECT 10
%define SYS_MUNMAP 11
%define SYS_EXIT  60

ld_raw_open:
    ; rdi=path, rsi=flags, rdx=mode
    mov rax, SYS_OPEN
    syscall
    ret

ld_raw_close:
    ; rdi=fd
    mov rax, SYS_CLOSE
    syscall
    ret

ld_raw_read:
    ; rdi=fd, rsi=buf, rdx=count
    mov rax, SYS_READ
    syscall
    ret

ld_raw_write:
    ; int64_t ld_raw_write(int64_t fd, const void* buf, uint64_t count)
    ; rdi = fd, rsi = buf, rdx = count
    mov rax, SYS_WRITE
    syscall
    ret

ld_raw_seek:
    ; rdi=fd, rsi=offset, rdx=whence
    mov rax, SYS_LSEEK
    syscall
    ret

ld_raw_exit:
    ; rdi=code
    mov rax, SYS_EXIT
    syscall
    hlt ; sollte nie erreicht werden

global ld_raw_mmap
ld_raw_mmap:
    ; int64_t ld_raw_mmap(void* addr, uint64_t length, int prot, int flags, int64_t fd, int64_t offset)
    ; rdi=addr, rsi=length, rdx=prot, rcx=flags, r8=fd, r9=offset
    mov r10, rcx
    mov rax, SYS_MMAP
    syscall
    ret

global ld_raw_mprotect
ld_raw_mprotect:
    ; int64_t ld_raw_mprotect(void* addr, uint64_t length, int prot)
    mov rax, SYS_MPROTECT
    syscall
    ret

global ld_raw_munmap
ld_raw_munmap:
    ; int64_t ld_raw_munmap(void* addr, uint64_t length)
    mov rax, SYS_MUNMAP
    syscall
    ret