// syscall_numbers.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 02.08.25.
//
// This file is part of VesperaOS.
// 
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#ifndef SYSCALL_NUMBERS_H
#define SYSCALL_NUMBERS_H

enum SyscallNumbers {
    SYSCALL_READ = 0,
    SYSCALL_WRITE = 1,
    SYSCALL_OPEN = 2,
    SYSCALL_CLOSE = 3,
    SYSCALL_STAT = 4,
    SYSCALL_CREATE = 11,
    SYSCALL_EXIT = 60,
    SYSCALL_GETCWD = 79,
    SYSCALL_CHDIR = 80,
    SYSCALL_RENAME = 82,
    SYSCALL_MKDIR = 83,
    SYSCALL_RMDIR = 84,
    SYSCALL_UNLINK = 87,
    SYSCALL_REBOOT = 169
};

#endif //SYSCALL_NUMBERS_H
