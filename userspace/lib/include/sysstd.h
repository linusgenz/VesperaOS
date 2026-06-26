// sysstd.h
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

#ifndef SYSSTD_H
#define SYSSTD_H

#include <stdint.h>

int64_t syscall(uint64_t num, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

/**
 * @brief Close a handle.
 *
 * @param hid Handle ID to close.
 * @return 0 on success, or -EBADH on invalid handle.
 */
int64_t sys_close(uint64_t hid, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Create a file.
 *
 * @param path Path of the file to create.
 * @return 0 on success, or negative error code (e.g., -EEXIST, -EINVAL).
 */
int64_t sys_create(uint64_t path_ptr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Terminate the current unit.
 *
 * @param code Exit code.
 * @return This function does not return; halts the unit.
 */
int64_t sys_exit(uint64_t code, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Perform an ioctl operation on a device handle.
 *
 * @param hid Handle ID.
 * @param request Device-specific request code.
 * @param arg Pointer to request argument.
 * @return Device-dependent result, or negative error code.
 */
int64_t sys_ioctl(uint64_t hid, uint64_t request, uint64_t arg, uint64_t, uint64_t, uint64_t);

/**
 * @brief Create a directory.
 *
 * @param path Directory path.
 * @return 0 on success, or negative error code (e.g., -EEXIST, -EINVAL).
 */
int64_t sys_mkdir(uint64_t path_ptr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Open a file or device.
 *
 * @param path_ptr pointer to the path of the file/device.
 * @param flags Open flags (O_RDONLY, O_WRONLY, O_RDWR).
 * @return Handle ID on success, or negative error code.
 */
int64_t sys_open(uint64_t path_ptr, uint64_t flags, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Read from a handle into a buffer.
 *
 * @param hid Handle ID.
 * @param buf Buffer pointer.
 * @param count Number of bytes to read.
 * @return Number of bytes read, or negative error code.
 */
int64_t sys_read(uint64_t hid, uint64_t buf_ptr, uint64_t count, uint64_t, uint64_t, uint64_t);

/**
 * @brief Reboot or power off the system.
 *
 * @param magic1 First magic number (must match REBOOT_MAGIC1).
 * @param magic2 Second magic number (must match REBOOT_MAGIC2).
 * @param cmd Reboot command (REBOOT_RESTART, REBOOT_POWER_OFF).
 * @return 0 on success, -1 on failure.
 */
int64_t sys_reboot(uint64_t magic1, uint64_t magic2, uint64_t cmd, uint64_t, uint64_t, uint64_t);

/**
 * @brief Rename a file or directory.
 *
 * @param oldPath Old path.
 * @param newPath New path.
 * @return 0 on success, or negative error code.
 */
int64_t sys_rename(uint64_t oldPath_ptr, uint64_t newPath_ptr, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Remove a directory.
 *
 * @param path Directory path.
 * @return 0 on success, or negative error code (e.g., -ENOTEMPTY).
 */
int64_t sys_rmdir(uint64_t path_ptr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @todo add doc
 */
int64_t sys_nanosleep(uint64_t ms, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Spawn a new realm/unit.
 *
 * @param path_ptr Path to binary.
 * @param argv_ptr Argument vector.
 * @return New realm ID on success, or negative error code.
 */
int64_t sys_spawn(uint64_t path_ptr, uint64_t argv_ptr, uint64_t envp, uint64_t cfg, uint64_t, uint64_t);

/**
 * @brief Unlink (delete) a file.
 *
 * @param path Path to the file.
 * @return 0 on success, or negative error code (e.g., -ENOENT).
 */
int64_t sys_unlink(uint64_t path_ptr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Write data to a handle.
 *
 * @param hid Handle ID.
 * @param buf Buffer pointer.
 * @param count Number of bytes to write.
 * @return Number of bytes written, or negative error code.
 */
int64_t sys_write(uint64_t hid, uint64_t buf_ptr, uint64_t count, uint64_t, uint64_t, uint64_t);

/**
 * @brief Reads the next entry in an open directory.
 *
 * @param arg0 Handle ID of the directory (returned by sys_open).
 * @param arg1 Pointer to a user-space buffer where the dirent_t will be copied.
 *             Must be at least sizeof(dirent_t) bytes.
 *
 * @return On success, returns 1 (entry was read).
 *         Returns 0 if there are no more entries.
 *         Returns negative errno on error:
 *           -EINVAL : Invalid parameters or buffer
 *           -EBADH  : Invalid handle
 *           -EACCES : Insufficient capabilities
 */
int64_t sys_readdir(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Blocks the calling unit until the target realm has finished execution.
 *
 *
 * @param arg0 RealmID of the realm to wait for
 * @param arg1 Pointer to an int where the exit status will be stored (optional; can be 0)
 * @return 0 on success.
 *         Negative errno on error:
 *           -EINVAL : Invalid parameters or no current unit
 *           -ECHILD : Target realm does not exist
 */
int64_t sys_wait(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Maps memory pages into the virtual address space of the calling unit.
 *
 * This syscall provides anonymous or file-backed memory regions to user programs.
 * It is the low-level primitive used to implement dynamic memory allocators
 * (e.g., malloc/free).
 *
 * @param arg0 Desired start address (or 0 for automatic placement).
 * @param arg1 Length of mapping in bytes (will be page-aligned).
 * @param arg2 Protection flags (PROT_READ, PROT_WRITE, etc.).
 * @param arg3 Mapping flags (MAP_ANONYMOUS, MAP_PRIVATE, etc.).
 * @param arg4 Handle for file-backed mappings (ignored if MAP_ANONYMOUS).
 * @param arg5 File offset in bytes (must be page-aligned).
 *
 * @return On success: starting virtual address of the new mapping.
 *         On error: negative errno value:
 *           -EINVAL       : Invalid arguments (length=0 or bad alignment)
 *           -EUNSUPPORTED : Unsupported flags or features
 *           -EACCES       : Not allowed in current context (e.g. kernel-only)
 *           -ENOMEM       : Out of memory (physical pages not available)
 */
int64_t sys_mmap(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

/**
 * @brief Unmaps memory pages from the virtual address space of the calling unit.
 *
 * This syscall removes a previously established mapping created by ::sys_mmap.
 * The specified range of virtual addresses is released, and future accesses
 * to the region will trigger a page fault.
 *
 * @param arg0 Starting virtual address of the mapping to remove (must be page-aligned).
 * @param arg1 Length of the region to unmap in bytes (will be page-aligned).
 *
 * @return On success: 0.
 *         On error: negative errno value:
 *           -EINVAL : Invalid arguments (unaligned address/length, or not mapped)
 *           -EACCES : Region cannot be unmapped (e.g., kernel-only mapping)
 */
int64_t sys_munmap(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t);

/**
 * @brief Adjust the program break (end of heap) for the current unit.
 *
 * This syscall sets the end of the user heap to the specified address.
 * If addr is 0, it simply returns the current break.
 *
 * @param arg0 New desired end of heap (0 to query current break)
 * @return On success, returns the new program break.
 *         On error, returns negative errno:
 *           -EACCES : when unit is no user unit (this error should never be thrown)
 *           -EINVAL : addr is below heap start or exceeds limit
 *           -ENOMEM : not enough memory to expand
 */
int64_t sys_brk(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Create a new channel with the given capacity.
 *
 * This syscall allocates a channel buffer and returns a handle
 * representing the channel in the current realm.
 *
 * @param arg0 Desired capacity of the channel buffer. If 0, a default
 *        capacity of 4096 bytes is used.
 * @return On success, returns a positive handle ID representing the channel.
 *         On error, returns negative errno:
 *           -EINVAL : invalid current unit or realm
 *           -ENOMEM : not enough memory to allocate channel
 */
int64_t sys_channel_create(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Receive data from a channel.
 *
 * Copies up to `len` bytes from the channel associated with the handle
 * into the provided buffer. This call may fail if the channel is empty.
 *
 * @param arg0 Handle ID of the channel to receive from.
 * @param arg1 Pointer to the user buffer to store received data.
 * @param arg2 Maximum number of bytes to receive.
 * @return On success, returns the number of bytes received.
 *         On error, returns negative errno:
 *           -EINVAL : invalid channel handle or resource
 *           -EBADH  : handle not found
 *           -EACCES : read capability missing
 *           -EAGAIN : channel is empty, try again later
 */
int64_t sys_channel_recv(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t);

/**
 * @brief Send data to a channel.
 *
 * Copies up to `len` bytes from the user buffer into the channel
 * associated with the handle. This call may fail if the channel is full.
 *
 * @param arg0 Handle ID of the channel to send to.
 * @param arg1 Pointer to the user buffer containing data to send.
 * @param arg2 Number of bytes to send.
 * @return On success, returns the number of bytes written to the channel.
 *         On error, returns negative errno:
 *           -EINVAL : invalid channel handle or resource
 *           -EBADH  : handle not found
 *           -EACCES : write capability missing
 *           -EAGAIN : channel is full, try again later
 */
int64_t sys_channel_send(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t);

/**
 * @brief Reposition the file offset for a file or device handle.
 *
 * Changes the current position within a file or device for subsequent
 * read/write operations. The new position is calculated based on the
 * whence parameter:
 *   - SEEK_SET (0): Set position to offset bytes from the beginning
 *   - SEEK_CUR (1): Set position to current position + offset
 *   - SEEK_END (2): Set position to file size + offset
 *
 * This operation is not supported for directories or TTY devices.
 *
 * @param arg0 Handle ID of the file or device to seek within.
 * @param arg1 Offset value (signed 64-bit integer).
 * @param arg2 Whence parameter (SEEK_SET, SEEK_CUR, or SEEK_END).
 * @return On success, returns the new absolute position from the beginning
 *         of the file. On error, returns negative errno:
 *           -EINVAL  : invalid handle, negative position, or invalid whence
 *           -EBADH   : handle not found or invalid resource
 *           -ESPIPE  : handle refers to a TTY or directory
 *           -EUNKNOWN: realm not found
 */
int64_t sys_seek(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t);

/**
 * @brief Change the current working directory of the calling realm.
 *
 * Resolves the given path (relative or absolute), normalizes any
 * "." and ".." components, and updates the realm's current working directory if the
 * target exists and is a directory.
 *
 * @param arg0 Pointer to a null-terminated path string (user address).
 * @return On success, returns 0.
 *         On error, returns negative errno:
 *           -EINVAL  : path is null or empty
 *           -ENOENT  : path does not exist
 *           -ENOTDIR : path exists but is not a directory
 *           -ERANGE  : resolved path exceeds cwd_path buffer size
 */
int64_t sys_chdir(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Get the current working directory of the calling realm.
 *
 * Copies the null-terminated absolute path of the realm's current
 * working directory into the provided user buffer. The buffer must
 * be large enough to hold the path including the null terminator.
 *
 * @param arg0 Pointer to the user buffer to store the path string.
 * @param arg1 Size of the user buffer in bytes.
 * @return On success, returns the number of bytes written (including
 *         the null terminator). On error, returns negative errno:
 *           -EINVAL  : buffer pointer is null or size is zero
 *           -ERANGE  : buffer is too small to hold the current path
 */
int64_t sys_getcwd(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Query metadata about a file or node.
 *
 * @param arg0 Pointer to path string.
 * @param arg1 Pointer to vespera_stat_t buffer to fill.
 * @return 0 on success, negative errno:
 *   -EINVAL : null path or buffer
 *   -EFAULT : buffer pointer invalid
 *   -ENOENT : path does not exist
 */
int64_t sys_stat(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Wait for events on a set of handles.
 *
 * Monitors the handles in @p hdls for the requested events. The call blocks
 * until at least one handle becomes ready, the timeout expires, or an error
 * occurs. Each entry in @p hdls describes one handle to watch and receives
 * the events that actually occurred in its @c revents field.
 *
 * Supported event flags:
 * | Flag     | Value | Meaning                          |
 * |----------|-------|----------------------------------|
 * | POLLIN   | 0x01  | Data available for reading       |
 * | POLLOUT  | 0x02  | Space available for writing      |
 * | POLLERR  | 0x04  | Error condition (output only)    |
 * | POLLHUP  | 0x08  | Handle closed / not found        |
 *
 * @param arg0 Pointer to an array of @c pollhdl structures. Each element
 *             specifies a handle ID and the events to watch for.
 *             The @c revents field of each element is filled by the kernel.
 * @param arg1 Number of @c pollhdl entries in the array pointed to by @p arg0.
 * @param arg2 Timeout in milliseconds:
 *             -  @c >0 : block for at most this many milliseconds
 *             -  @c  0 : return immediately (non-blocking check)
 *             -  @c -1 : block indefinitely until an event occurs
 *
 * @return On success, returns the number of handles with non-zero @c revents
 *         (i.e. handles that are ready or have an error/hangup).
 *         Returns @c 0 if the timeout expired before any handle became ready.
 *         On error, returns negative errno:
 *           -EINVAL : @p hdls is null or @p nhdls is zero
 *           -EFAULT : @p hdls pointer is not accessible
 */
int64_t sys_poll(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t);

int64_t sys_pipe(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_getrid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_getunid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_mount(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t, uint64_t);

int64_t sys_umount(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_kill(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_sigaction(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_clock_gettime(uint64_t clk_id, uint64_t ts, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Subscribe the calling realm to vbus events.
 *
 * @param arg0 Pointer to vbus_subscribe_args_t.
 * @return 0 on success, negative errno on failure.
 */
int64_t sys_vbus_subscribe(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Remove all vbus subscriptions for the calling realm.
 *
 * @return 0 on success.
 */
int64_t sys_vbus_unsubscribe(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Spawn a new unit (thread) inside an existing realm.
 *
 * The new unit shares the realm's virtual address space (page table) and
 * starts executing at @p entry with @p arg_ptr in RDI. It gets its own
 * kernel stack and user stack.
 *
 * @param arg0  realm_id   - target realm ID; 0 = caller's own realm.
 * @param arg1  entry      - user-space virtual address of the entry function.
 *                           The function must have signature  void fn(void*).
 * @param arg2  arg_ptr    - opaque value forwarded as the first argument (RDI)
 *                           to the entry function.
 * @param arg3  stack_size - user-stack size in bytes; 0 = kernel default. may not be smaller than 16kb and not larger
 * than 8mb
 * @param arg4  flags      - reserved, must be 0.
 *
 * @return New UnitID (> 0) on success, or negative errno:
 *   -EINVAL  : entry is NULL, flags != 0, or no current unit
 *   -EACCES  : caller does not own the target realm
 *   -ECHILD  : target realm does not exist or is inactive
 *   -ENOMEM  : could not allocate stack or unit slot
 */
int64_t sys_unit_spawn(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t);

int64_t sys_gettimeofday(uint64_t clk_id, uint64_t ts, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_time(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_clock_nanosleep(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

/** Copies a handle from the calling realm into another realm.
 * The underlying resource is shared — the same Channel* object is
 * registered in both handle tables, with its refcount incremented.
 *
 * Only handles marked @code transferable = true@endcode may be transferred.
 * The caller may restrict the capability set: the target never gets
 * more capabilities than the source entry holds.
 *
 * @param arg0  hid             Handle ID in the calling realm.
 * @param arg1  target_realm_id The destination realm.
 * @param arg2  caps_mask       Capability bits to grant (subset of source caps).
 *                              Pass CAP_ALL (0xFF…) to forward all source caps.
 *
 * @return  The new HandleId in the target realm on success, or negative errno:
 *   -EINVAL  : invalid arguments or no current unit
 *   -EBADH   : hid not found in calling realm
 *   -EACCES  : handle is not transferable, or caps_mask exceeds source caps
 *   -ECHILD  : target_realm_id does not exist
 *   -ENOMEM  : no free slot in target handle table
 */
int64_t sys_handle_transfer(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t);

int64_t sys_setsid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_setpgid(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_getpgid(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_tcsetpgrp(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_tcgetpgrp(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Get the real user ID of the calling realm.
 *
 * @return The real user ID (always succeeds).
 */
int64_t sys_getuid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Get the effective user ID of the calling realm.
 *
 * @return The effective user ID (always succeeds).
 */
int64_t sys_geteuid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Set the user ID of the calling realm.
 *
 * POSIX semantics:
 *   - If euid == 0 (root): sets uid, euid, and suid to @p arg0.
 *   - Otherwise: may only set euid to the current uid or suid (privilege drop/restore).
 *
 * @param arg0 The target user ID.
 * @return On success, returns 0.
 *         On error, returns negative errno:
 *           -ESRCH  : no current realm found
 *           -EPERM  : caller is not root and @p arg0 is not uid or suid
 */
int64_t sys_setuid(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Set the real and effective user IDs independently.
 *
 * Pass (uint32_t)-1 for a field to leave it unchanged.
 * The saved set-user-ID is updated to the new euid whenever ruid changes
 * or euid is set to a value other than the old real uid.
 *
 * @param arg0 New real user ID, or (uint32_t)-1 to leave unchanged.
 * @param arg1 New effective user ID, or (uint32_t)-1 to leave unchanged.
 * @return On success, returns 0.
 *         On error, returns negative errno:
 *           -ESRCH  : no current realm found
 *           -EPERM  : unprivileged caller attempted a disallowed change
 */
int64_t sys_setreuid(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Set the real, effective, and saved set-user-IDs.
 *
 * Each field may be set independently. Pass (uint32_t)-1 to leave a field unchanged.
 * A non-root caller may only set each field to one of its current uid, euid, or suid.
 *
 * @param arg0 New real user ID, or (uint32_t)-1 to leave unchanged.
 * @param arg1 New effective user ID, or (uint32_t)-1 to leave unchanged.
 * @param arg2 New saved set-user-ID, or (uint32_t)-1 to leave unchanged.
 * @return On success, returns 0.
 *         On error, returns negative errno:
 *           -ESRCH  : no current realm found
 *           -EPERM  : one or more requested values are not permitted
 */
int64_t sys_setresuid(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t);

/**
 * @brief Get the real, effective, and saved set-user-IDs.
 *
 * @param arg0 Pointer to uint32_t to receive the real user ID.
 * @param arg1 Pointer to uint32_t to receive the effective user ID.
 * @param arg2 Pointer to uint32_t to receive the saved set-user-ID.
 * @return On success, returns 0.
 *         On error, returns negative errno:
 *           -EINVAL : one or more output pointers are null
 *           -ESRCH  : no current realm found
 */
int64_t sys_getresuid(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t);

/**
 * @brief Get the real group ID of the calling realm.
 *
 * @return The real group ID (always succeeds).
 */
int64_t sys_getgid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Get the effective group ID of the calling realm.
 *
 * @return The effective group ID (always succeeds).
 */
int64_t sys_getegid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Set the group ID of the calling realm.
 *
 * POSIX semantics:
 *   - If euid == 0 (root): sets gid, egid, and sgid to @p arg0.
 *   - Otherwise: may only set egid to the current gid or sgid.
 *
 * @param arg0 The target group ID.
 * @return On success, returns 0.
 *         On error, returns negative errno:
 *           -ESRCH  : no current realm found
 *           -EPERM  : caller is not root and @p arg0 is not gid or sgid
 */
int64_t sys_setgid(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Set the real and effective group IDs independently.
 *
 * Pass (uint32_t)-1 for a field to leave it unchanged.
 * The saved set-group-ID is updated to the new egid whenever rgid changes
 * or egid is set to a value other than the old real gid.
 *
 * @param arg0 New real group ID, or (uint32_t)-1 to leave unchanged.
 * @param arg1 New effective group ID, or (uint32_t)-1 to leave unchanged.
 * @return On success, returns 0.
 *         On error, returns negative errno:
 *           -ESRCH  : no current realm found
 *           -EPERM  : unprivileged caller attempted a disallowed change
 */
int64_t sys_setregid(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Set the real, effective, and saved set-group-IDs.
 *
 * Each field may be set independently. Pass (uint32_t)-1 to leave a field unchanged.
 * A non-root caller may only set each field to one of its current gid, egid, or sgid.
 *
 * @param arg0 New real group ID, or (uint32_t)-1 to leave unchanged.
 * @param arg1 New effective group ID, or (uint32_t)-1 to leave unchanged.
 * @param arg2 New saved set-group-ID, or (uint32_t)-1 to leave unchanged.
 * @return On success, returns 0.
 *         On error, returns negative errno:
 *           -ESRCH  : no current realm found
 *           -EPERM  : one or more requested values are not permitted
 */
int64_t sys_setresgid(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t);

/**
 * @brief Get the real, effective, and saved set-group-IDs.
 *
 * @param arg0 Pointer to uint32_t to receive the real group ID.
 * @param arg1 Pointer to uint32_t to receive the effective group ID.
 * @param arg2 Pointer to uint32_t to receive the saved set-group-ID.
 * @return On success, returns 0.
 *         On error, returns negative errno:
 *           -EINVAL : one or more output pointers are null
 *           -ESRCH  : no current realm found
 */
int64_t sys_getresgid(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t);

/**
 * @brief Change the owner and group of a file by path.
 *
 * A non-root caller may not change the owner (uid) of a file.
 * A non-root caller may only change the group to their own egid or gid.
 *
 * @param arg0 Pointer to a null-terminated path string (user address).
 * @param arg1 New owner user ID.
 * @param arg2 New owner group ID.
 * @return On success, returns 0.
 *         On error, returns negative errno:
 *           -EINVAL : path is null
 *           -ESRCH  : no current realm found
 *           -ENOENT : path does not exist
 *           -EPERM  : caller lacks permission to change owner or group
 */
int64_t sys_chown(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t);

/**
 * @brief Change the owner and group of an open file by handle.
 *
 * Same permission rules as sys_chown, but operates on an already-open handle
 * instead of a path.
 *
 * @param arg0 Handle ID of the open file or directory.
 * @param arg1 New owner user ID.
 * @param arg2 New owner group ID.
 * @return On success, returns 0.
 *         On error, returns negative errno:
 *           -ESRCH  : no current realm found
 *           -EBADH  : handle is invalid or not a file/directory handle
 *           -EPERM  : caller lacks permission to change owner or group
 */
int64_t sys_fchown(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t);

/**
 * @brief Change the permission mode bits of a file by path.
 *
 * Only the file owner or root may change the mode. The setuid/setgid bits
 * in @p arg1 are silently cleared for non-root callers if the file's group
 * does not match the caller's egid.
 *
 * @param arg0 Pointer to a null-terminated path string (user address).
 * @param arg1 New mode bits (lower 12 bits used: setuid, setgid, sticky, rwxrwxrwx).
 * @return On success, returns 0.
 *         On error, returns negative errno:
 *           -EINVAL : path is null
 *           -ESRCH  : no current realm found
 *           -ENOENT : path does not exist
 *           -EPERM  : caller is not the file owner or root
 */
int64_t sys_chmod(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Change the permission mode bits of an open file by handle.
 *
 * Same permission rules as sys_chmod, but operates on an already-open handle
 * instead of a path.
 *
 * @param arg0 Handle ID of the open file or directory.
 * @param arg1 New mode bits (lower 12 bits used: setuid, setgid, sticky, rwxrwxrwx).
 * @return On success, returns 0.
 *         On error, returns negative errno:
 *           -ESRCH  : no current realm found
 *           -EBADH  : handle is invalid or not a file/directory handle
 *           -EPERM  : caller is not the file owner or root
 */
int64_t sys_fchmod(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

/**
 * @brief Low-level Syscall-Wrapper für Shared Memory und Handle-Management
 */
int64_t sys_shm_open(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);
int64_t sys_shm_unlink(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);
int64_t sys_handle_truncate(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

int64_t sys_vbus_emit(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t);

int64_t sys_chronos_summary(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_chronos_checkpoint(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_join_unit(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_yield(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_get_unid(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_futex(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

int64_t sys_chroot(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);

int64_t sys_dup(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t sys_dup2(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t);
int64_t sys_dup3(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t);


#endif  // SYSSTD_H
