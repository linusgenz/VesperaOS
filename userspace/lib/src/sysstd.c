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
#define SYSCALL_FSTAT 5
#define SYSCALL_SEEK 8
#define SYSCALL_POLL 7
#define SYSCALL_MMAP 9
#define SYSCALL_MPROTECT 10
#define SYSCALL_MUNMAP 11
#define SYSCALL_BRK 12
#define SYSCALL_SIGACTION 13
#define SYSCALL_SIGPROCMASK 14
#define SYSCALL_SIGRETURN 15
#define SYSCALL_IOCTL 16
#define SYSCALL_PIPE 22
#define SYSCALL_YIELD     24
#define SYSCALL_DUP       32
#define SYSCALL_DUP2       33
#define SYSCALL_NANOSLEEP 35
#define SYSCALL_GETRID 39
#define SYSCALL_UNIT_SPAWN 59
#define SYSCALL_EXIT 60
#define SYSCALL_WAIT 61
#define SYSCALL_KILL 62
#define SYSCALL_SPAWN 69
#define SYSCALL_GETCWD 79
#define SYSCALL_CHDIR 80
#define SYSCALL_RENAME 82
#define SYSCALL_MKDIR 83
#define SYSCALL_RMDIR 84
#define SYSCALL_CREATE 85
#define SYSCALL_UNLINK 87
#define SYSCALL_GETTIMEOFDAY 96
#define SYSCALL_SYSINFO 99
#define SYSCALL_CHROOT 161
#define SYSCALL_MOUNT 165
#define SYSCALL_UMOUNT 166
#define SYSCALL_REBOOT 169
#define SYSCALL_GETUNID 186
#define SYSCALL_TIME 201
#define SYSCALL_FUTEX    202
#define SYSCALL_SCHED_SETAFFINITY 203
#define SYSCALL_SCHED_GETAFFINITY 204
#define SYSCALL_READDIR 217
#define SYSCALL_CLOCK_GETTIME 228
#define SYSCALL_CLOCK_NANOSLEEP 230
#define SYSCALL_OPENAT 257
#define SYSCALL_DUP3 292
#define SYSCALL_GETUID 102
#define SYSCALL_GETEUID 107
#define SYSCALL_SETUID 105
#define SYSCALL_SETREUID 113
#define SYSCALL_SETRESUID 117
#define SYSCALL_GETRESUID 118
#define SYSCALL_GETGID 104
#define SYSCALL_GETEGID 108
#define SYSCALL_SETGID 106
#define SYSCALL_SETREGID 114
#define SYSCALL_SETRESGID 119
#define SYSCALL_GETRESGID 120
#define SYSCALL_CHOWN 92
#define SYSCALL_FCHOWN 93
#define SYSCALL_CHMOD 90
#define SYSCALL_FCHMOD 91

#define SYSCALL_CHANNEL_CREATE 130
#define SYSCALL_CHANNEL_SEND 131
#define SYSCALL_CHANNEL_RECEIVE 132
#define SYSCALL_MKNOD  133
#define SYSCALL_MKNODAT  259
#define SYSCALL_HANDLE_TRANSFER 135

#define SYSCALL_SETSID 140
#define SYSCALL_SETPGID 141
#define SYSCALL_GETPGID 142
#define SYSCALL_TCSETPGRP 143
#define SYSCALL_TCGETPGRP 144

#define SYSCALL_EXIT_GROUP 231

#define SYSCALL_VBUS_SUBSCRIBE     300
#define SYSCALL_VBUS_UNSUBSCRIBE   301
#define SYSCALL_VBUS_EMIT   302

#define SYSCALL_SHM_OPEN       303
#define SYSCALL_SHM_UNLINK       304
#define SYSCALL_HANDLE_TRUNCATE       305

#define SYSCALL_CHRONOS_CHECKPOINT 321
#define SYSCALL_CHRONOS_SUMMARY 322

#define SYSCALL_JOIN_UNIT 323


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

int64_t sys_open(uint64_t path_ptr, uint64_t flags, uint64_t mode, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_OPEN, path_ptr, flags, mode, 0, 0, 0);
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

int64_t sys_nanosleep(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_NANOSLEEP, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_exit(uint64_t code, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_EXIT, code, 0, 0, 0, 0, 0);
}

int64_t sys_spawn(uint64_t path_ptr, uint64_t argv_ptr, uint64_t envp, uint64_t cfg, uint64_t, uint64_t) {
    return syscall(SYSCALL_SPAWN, path_ptr, argv_ptr, envp, cfg, 0, 0);
}

int64_t sys_rename(uint64_t oldPath_ptr, uint64_t newPath_ptr, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_RENAME, oldPath_ptr, newPath_ptr, 0, 0, 0, 0);
}

int64_t sys_mkdir(uint64_t path_ptr, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_MKDIR, path_ptr, arg1, 0, 0, 0, 0);
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

int64_t sys_wait(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_WAIT, arg0, arg1, arg2, 0, 0, 0);
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

int64_t sys_pipe(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_PIPE, arg0, 0, 0, 0, 0, 0);
}

int64_t sys_getrid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_GETRID, 0, 0, 0, 0, 0, 0);
}

int64_t sys_getunid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_GETUID, 0, 0, 0, 0, 0, 0);
}

int64_t sys_mount(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t, uint64_t) {
    return syscall(SYSCALL_MOUNT, arg0, arg1, arg2, arg3, 0, 0);
}

int64_t sys_umount(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_UMOUNT, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_kill(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_KILL, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_sigaction(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_SIGACTION, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_sigreturn(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_SIGRETURN, 0, 0, 0, 0, 0, 0);
}

int64_t sys_clock_gettime(uint64_t clk_id, uint64_t ts, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_CLOCK_GETTIME, clk_id, ts, 0, 0, 0, 0);
}

int64_t sys_vbus_subscribe(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_VBUS_SUBSCRIBE, arg0, 0, 0, 0, 0, 0);
}

int64_t sys_vbus_unsubscribe(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_VBUS_UNSUBSCRIBE, 0, 0, 0, 0, 0, 0);
}

int64_t sys_unit_spawn(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t) {
    return syscall(SYSCALL_UNIT_SPAWN, arg0, arg1, arg2, arg3, arg4, 0);
}

int64_t sys_gettimeofday(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_GETTIMEOFDAY, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_time(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_TIME, arg0, 0, 0, 0, 0, 0);
}

int64_t sys_clock_nanosleep(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t, uint64_t) {
    return syscall(SYSCALL_CLOCK_NANOSLEEP, arg0, arg1, arg2, arg3, 0, 0);
}

int64_t sys_handle_transfer(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_HANDLE_TRANSFER, arg0, arg1, arg2, 0, 0, 0);
}

int64_t sys_setsid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_SETSID, 0, 0, 0, 0, 0, 0);
}

int64_t sys_setpgid(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_SETPGID, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_getpgid(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_GETPGID, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_tcsetpgrp(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_TCSETPGRP, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_tcgetpgrp(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_TCGETPGRP, arg0, 0, 0, 0, 0, 0);
}

int64_t sys_getuid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_GETUID, 0, 0, 0, 0, 0, 0);
}

int64_t sys_geteuid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_GETEUID, 0, 0, 0, 0, 0, 0);
}

int64_t sys_setuid(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_SETUID, arg0, 0, 0, 0, 0, 0);
}

int64_t sys_setreuid(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_SETREUID, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_setresuid(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_SETRESUID, arg0, arg1, arg2, 0, 0, 0);
}

int64_t sys_getresuid(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_GETRESUID, arg0, arg1, arg2, 0, 0, 0);
}

int64_t sys_getgid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_GETGID, 0, 0, 0, 0, 0, 0);
}

int64_t sys_getegid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_GETEGID, 0, 0, 0, 0, 0, 0);
}

int64_t sys_setgid(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_SETGID, arg0, 0, 0, 0, 0, 0);
}

int64_t sys_setregid(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_SETREGID, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_setresgid(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_SETRESGID, arg0, arg1, arg2, 0, 0, 0);
}

int64_t sys_getresgid(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_GETRESGID, arg0, arg1, arg2, 0, 0, 0);
}

int64_t sys_chown(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_CHOWN, arg0, arg1, arg2, 0, 0, 0);
}

int64_t sys_fchown(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_FCHOWN, arg0, arg1, arg2, 0, 0, 0);
}

int64_t sys_chmod(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_CHMOD, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_fchmod(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_FCHMOD, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_shm_open(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    return syscall(SYSCALL_SHM_OPEN, arg0, arg1, arg2, arg3, arg4, arg5);
}

int64_t sys_shm_unlink(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    return syscall(SYSCALL_SHM_UNLINK, arg0, arg1, arg2, arg3, arg4, arg5);
}

int64_t sys_handle_truncate(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    return syscall(SYSCALL_HANDLE_TRUNCATE, arg0, arg1, arg2, arg3, arg4, arg5);
}

int64_t sys_vbus_emit(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_VBUS_EMIT, arg0, arg1, arg2, 0, 0, 0);
}

int64_t sys_chronos_checkpoint(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_CHRONOS_CHECKPOINT, arg0, 0, 0, 0, 0, 0);
}

int64_t sys_chronos_summary(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_CHRONOS_SUMMARY, 0, 0, 0, 0, 0, 0);
}

int64_t sys_join_unit(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_JOIN_UNIT, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_yield(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_YIELD, 0, 0, 0, 0, 0, 0);
}

int64_t sys_get_unid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_GETUNID, 0, 0, 0, 0, 0, 0);
}

int64_t sys_futex(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    return syscall(SYSCALL_FUTEX, arg0, arg1, arg2, arg3, arg4, arg5);
}

int64_t sys_chroot(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_CHROOT, arg0, 0, 0, 0, 0, 0);
}

int64_t sys_dup(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_DUP, arg0, 0, 0, 0, 0, 0);
}

int64_t sys_dup2(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_DUP2, arg0, arg1, 0, 0, 0, 0);
}

int64_t sys_dup3(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_DUP3, arg0, arg1, arg2, 0, 0, 0);
}

int64_t sys_mprotect(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_MPROTECT, arg0, arg1, arg2, 0, 0, 0);
}

int64_t sys_exit_group(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_EXIT_GROUP, arg0, 0, 0, 0, 0, 0);
}

int64_t sys_fstat(uint64_t fd, uint64_t buf, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_FSTAT, fd, buf, 0, 0, 0, 0);
}

int64_t sys_sysinfo(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_SYSINFO, arg0, 0, 0, 0, 0, 0);
}

int64_t sys_sched_getaffinity(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_SCHED_GETAFFINITY, arg0, arg1, arg2, 0, 0, 0);
}

int64_t sys_sigprocmask(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_SIGPROCMASK, arg0, arg1, arg2, 0, 0, 0);
}

int64_t sys_openat(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t, uint64_t) {
    return syscall(SYSCALL_OPENAT, arg0, arg1, arg2, arg3, 0, 0);
}

int64_t sys_mknod(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
    return syscall(SYSCALL_MKNOD, arg0, arg1, arg2, 0, 0, 0);
}

int64_t sys_mknodat(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t, uint64_t) {
    return syscall(SYSCALL_MKNODAT, arg0, arg1, arg2, arg3, 0, 0);
}