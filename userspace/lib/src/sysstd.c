// sysstd.c
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 22.09.25.
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

#include <sysstd.h>

#define SYSCALL_READ 0
#define SYSCALL_WRITE 1
#define SYSCALL_OPEN 2
#define SYSCALL_CLOSE 3
#define SYSCALL_STAT 4
#define SYSCALL_SEEK 8
#define SYSCALL_POLL 7
#define SYSCALL_MMAP 9
#define SYSCALL_MUNMAP 11
#define SYSCALL_BRK 12
#define SYSCALL_CREATE 13
#define SYSCALL_IOCTL 16
#define SYSCALL_SLEEP 35
#define SYSCALL_EXIT 60
#define SYSCALL_WAIT 61
#define SYSCALL_SPAWN 69
#define SYSCALL_GETCWD 79
#define SYSCALL_CHDIR 80
#define SYSCALL_RENAME 82
#define SYSCALL_MKDIR 83
#define SYSCALL_RMDIR 84
#define SYSCALL_UNLINK 87
#define SYSCALL_REBOOT 169
#define SYSCALL_READDIR 217

#define SYSCALL_CHANNEL_CREATE 130
#define SYSCALL_CHANNEL_SEND 131
#define SYSCALL_CHANNEL_RECEIVE 132

int64_t syscall(
    uint64_t num, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5
) {
    int64_t ret = -1;

    register uint64_t r10_ asm("r10") = arg3;
    register uint64_t r8_ asm("r8") = arg4;
    register uint64_t r9_ asm("r9") = arg5;

    asm volatile("syscall"
                 : "=a"(ret)
                 : "a"(num), "D"(arg0), "S"(arg1), "d"(arg2), "r"(r10_), "r"(r8_), "r"(r9_)
                 : "rcx", "r11", "memory");

    return ret;
}

int64_t sys_read(uint64_t hid, uint64_t buf_ptr, uint64_t count, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_READ, hid, buf_ptr, count, 0, 0, 0);
}

int64_t sys_write(uint64_t hid, uint64_t buf_ptr, uint64_t count, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_WRITE, hid, buf_ptr, count, 0, 0, 0);
}

int64_t sys_open(uint64_t path_ptr, uint64_t flags, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_OPEN, path_ptr, flags, 0, 0, 0, 0);
}

int64_t sys_close(uint64_t hid, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_CLOSE, hid, 0, 0, 0, 0, 0);
}

int64_t sys_create(uint64_t path_ptr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_CREATE, path_ptr, 0, 0, 0, 0, 0);
}

int64_t sys_ioctl(uint64_t hid, uint64_t request, uint64_t arg, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_IOCTL, hid, request, arg, 0, 0, 0);
}

int64_t sys_sleep(uint64_t ms, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_SLEEP, ms, 0, 0, 0, 0, 0);
}

int64_t sys_exit(uint64_t code, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_EXIT, code, 0, 0, 0, 0, 0);
}

int64_t sys_spawn(uint64_t path_ptr, uint64_t argv_ptr, uint64_t envp, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_SPAWN, path_ptr, argv_ptr, envp, 0, 0, 0);
}

int64_t sys_rename(uint64_t oldPath_ptr, uint64_t newPath_ptr, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_RENAME, oldPath_ptr, newPath_ptr, 0, 0, 0, 0);
}

int64_t sys_mkdir(uint64_t path_ptr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_MKDIR, path_ptr, 0, 0, 0, 0, 0);
}

int64_t sys_rmdir(uint64_t path_ptr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_RMDIR, path_ptr, 0, 0, 0, 0, 0);
}

int64_t sys_unlink(uint64_t path_ptr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_UNLINK, path_ptr, 0, 0, 0, 0, 0);
}

int64_t sys_reboot(uint64_t magic1, uint64_t magic2, uint64_t cmd, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_REBOOT, magic1, magic2, cmd, 0, 0, 0);
}

int64_t sys_readdir(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_READDIR, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_wait(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_WAIT, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_mmap(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    return syscall(SYSCALL_MMAP, arg0, arg1, arg2, arg3, arg4, arg5);
}

int64_t sys_munmap(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_MUNMAP, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_brk(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_BRK, arg0, 0, 0, 0, 0, 0);
}

int64_t sys_channel_create(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_CHANNEL_CREATE, arg0, 0, 0, 0, 0, 0);
}

int64_t sys_channel_recv(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_CHANNEL_RECEIVE, arg0, arg1, arg2, 0, 0, 0);
}

int64_t sys_channel_send(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_CHANNEL_SEND, arg0, arg1, arg2, 0, 0, 0);
}

int64_t sys_seek(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_SEEK, arg0, arg1, arg2, 0, 0, 0);
}

int64_t sys_chdir(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_CHDIR, arg0, 0, 0, 0, 0, 0);
}

int64_t sys_getcwd(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_GETCWD, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_stat(uint64_t path, uint64_t buf, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_STAT, path, buf, 0, 0, 0, 0);
}

int64_t sys_poll(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_POLL, arg0, arg1, arg2, 0, 0, 0);
}