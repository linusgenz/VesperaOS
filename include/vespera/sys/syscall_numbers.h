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
#define SYSCALL_POLL      7
#define SYSCALL_SEEK      8
#define SYSCALL_MMAP      9
#define SYSCALL_MUNMAP    11
#define SYSCALL_BRK       12
#define SYSCALL_SIGACTION 13
#define SYSCALL_SIGRETURN 15
#define SYSCALL_IOCTL     16
#define SYSCALL_PIPE      22
#define SYSCALL_NANOSLEEP 35
#define SYSCALL_GETRID    39
#define SYSCALL_UNIT_SPAWN 59
#define SYSCALL_EXIT      60
#define SYSCALL_WAIT      61
#define SYSCALL_KILL      62
#define SYSCALL_SPAWN     69
#define SYSCALL_GETCWD    79
#define SYSCALL_CHDIR     80
#define SYSCALL_RENAME    82
#define SYSCALL_MKDIR     83
#define SYSCALL_RMDIR     84
#define SYSCALL_CREATE    85
#define SYSCALL_UNLINK    87
#define SYSCALL_GETTIMEOFDAY 96
#define SYSCALL_MOUNT     165
#define SYSCALL_UMOUNT    166
#define SYSCALL_REBOOT    169
#define SYSCALL_GETUNID    186
#define SYSCALL_READDIR   217
#define SYSCALL_CLOCK_GETTIME 228

#define SYSCALL_GETUID      102
#define SYSCALL_GETEUID     107
#define SYSCALL_SETUID      105
#define SYSCALL_SETREUID    113
#define SYSCALL_SETRESUID   117
#define SYSCALL_GETRESUID   118
#define SYSCALL_GETGID      104
#define SYSCALL_GETEGID     108
#define SYSCALL_SETGID      106
#define SYSCALL_SETREGID    114
#define SYSCALL_SETRESGID   119
#define SYSCALL_GETRESGID   120
#define SYSCALL_CHOWN       92
#define SYSCALL_FCHOWN      93
#define SYSCALL_CHMOD       90
#define SYSCALL_FCHMOD      91

#define SYS_UNIT_SPAWN       100
#define SYS_UNIT_TERMINATE   101
#define SYS_UNIT_AWAIT       102
#define SYS_UNIT_SLEEP       103

#define SYS_REALM_CREATE     110
#define SYS_REALM_DESTROY    111
#define SYS_REALM_GET_INFO   112

#define SYS_HANDLE_CLOSE     121
#define SYS_HANDLE_DUPLICATE 122
#define SYS_HANDLE_GET_INFO  123

#define SYSCALL_CHANNEL_CREATE   130
#define SYSCALL_CHANNEL_SEND     131
#define SYSCALL_CHANNEL_RECEIVE  132
#define SYSCALL_HANDLE_TRANSFER  133

#define SYSCALL_SETSID      140
#define SYSCALL_SETPGID     141
#define SYSCALL_GETPGID     142
#define SYSCALL_TCSETPGRP   143
#define SYSCALL_TCGETPGRP   144

#define SYSCALL_TIME 201
#define SYSCALL_CLOCK_NANOSLEEP 230


#define SYSCALL_VBUS_SUBSCRIBE     300
#define SYSCALL_VBUS_UNSUBSCRIBE   301

#endif //SYSCALL_NUMBERS_H
