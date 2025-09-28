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

#define SYSCALL_READ      0
#define SYSCALL_WRITE     1
#define SYSCALL_OPEN      2
#define SYSCALL_CLOSE     3
#define SYSCALL_STAT      4
#define SYSCALL_MMAP      9
#define SYSCALL_MUNMAP    11
#define SYSCALL_BRK      12
#define SYSCALL_CREATE    13
#define SYSCALL_IOCTL     16
#define SYSCALL_SLEEP     35
//#define SYSCALL_GETPID    39
#define SYSCALL_EXIT      60
#define SYSCALL_WAIT      61
#define SYSCALL_SPAWN     69
#define SYSCALL_GETCWD    79
#define SYSCALL_CHDIR     80
#define SYSCALL_RENAME    82
#define SYSCALL_MKDIR     83
#define SYSCALL_RMDIR     84
#define SYSCALL_UNLINK    87
#define SYSCALL_REBOOT    169
#define SYSCALL_READDIR   217

// new

#define SYS_UNIT_SPAWN       100
#define SYS_UNIT_TERMINATE   101
#define SYS_UNIT_AWAIT       102
#define SYS_UNIT_SLEEP       103

#define SYS_REALM_CREATE     110
#define SYS_REALM_DESTROY    111
#define SYS_REALM_GET_INFO   112

#define SYS_HANDLE_TRANSFER  120
#define SYS_HANDLE_CLOSE     121
#define SYS_HANDLE_DUPLICATE 122
#define SYS_HANDLE_GET_INFO  123

#define SYS_CHANNEL_CREATE   130
#define SYS_CHANNEL_SEND     131
#define SYS_CHANNEL_RECEIVE  132

#define SYS_FILE_OPEN 140
#define SYS_FILE_READ 141
#define SYS_FILE_WRITE 142

#endif //SYSCALL_NUMBERS_H
