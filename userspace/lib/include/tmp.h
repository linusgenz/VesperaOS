// tmp.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 24.06.26.
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
#ifndef VESPERAWORKSPACE_TMP_H
#define VESPERAWORKSPACE_TMP_H
#include <stdint.h>

typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef uint64_t RealmID;  ///< type representing a realm identifier
typedef uint64_t UnitID;   ///< type representing a unit (thread) identifier

int chdir(const char* path);
int chroot(const char* path);

int64_t sys_setpgid(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t sys_setuid(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t sys_setsid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

typedef struct spawn_config {
    u64 stdin_handle;   ///< Replace HANDLE_STDIN  in child (0 = inherit TTY)
    u64 stdout_handle;  ///< Replace HANDLE_STDOUT in child (0 = inherit TTY)
    u64 stderr_handle;  ///< Replace HANDLE_STDERR in child (0 = inherit TTY)
    u8  bg_realm;       ///< If set true, detach from controlling tty
    char* realm_name;   ///< Optional explicit name for the new realm
    u32 uid;            ///< Real user ID for the new process (0 = inherit)
    u32 gid;            ///< Real group ID for the new process (0 = inherit)
    char* home;         ///< Optional home directory (sets initial cwd if provided)
} spawn_config_t;

/**
 * @brief Spawn a new realm (isolated execution context).
 *
 * The new realm starts with a single initial unit.
 *
 * @param path_ptr Pointer to the path of the executable binary.
 * @param argv Pointer to an array of argument strings.
 * @param envp Pointer to a NULL-terminated array of strings representing the environment variables for the new realm.
 * Can be @c NULL if no environment variables are needed.
 * @return Realm ID on success, negative error code on failure.
 */
RealmID spawn_realm(const char* path_ptr, char* const argv[], char* const envp[], spawn_config_t* cfg);

/**
 * @brief Terminate an entire realm and all its units.
 *
 * @param realm_id The ID of the realm to terminate.
 * @param code Exit code for the realm.
 * @return 0 on success, negative error code on failure.
 */
int64_t exit_realm(RealmID realm_id, uint64_t code);

#define WAIT_FLAG_NONE    0x0   // Standard: waiting, blocking
#define WAIT_FLAG_NOHANG  0x1   // return immediately if you're not finished yet

/**
 * @brief Wait for a realm to finish execution.
 *
 * Blocks the current process until the realm with ID @p realm_id has completed.
 *
 * @param realm_id The ID of the realm to wait for.
 * @param status Pointer to an int to store the exit status, or @c NULL if unused.
 * @return 0 on success, or a negative error code on failure (e.g., -ECHILD).
 */
int wait_realm(RealmID realm_id, int* status, uint32_t flags);

/**
 * @brief Send a signal to another process.
 *
 * @param pid    Target process ID.
 * @param signum Signal number.
 * @return 0 on success, -1 on error (errno is set).
 */
int kill(int pid, int signum);

int64_t sys_pipe(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_dup(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t sys_dup2(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t sys_dup3(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t);

/**
 * @brief Close a handle.
 *
 * @param hid Handle ID to close.
 * @return 0 on success, or -EBADH on invalid handle.
 */
int64_t sys_close(uint64_t hid, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

#endif //VESPERAWORKSPACE_TMP_H

