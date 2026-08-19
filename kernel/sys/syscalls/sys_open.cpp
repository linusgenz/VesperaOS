// sys_open.cpp
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

#include <filesystem/vfs.h>
#include <filesystem/vfs_node.h>
#include <security/permission.h>
#include <uapi/vespera/fcntl.h>
#include <uapi/vespera/handles.h>
#include <vespera/ipc/channel.h>
#include <vespera/realm/handles.h>
#include <vespera/scheduling.h>
#include <units/unit.h>
#include "filesystem/vfs_handle.h"
#include "sys/handle_resolution.h"
#include "vespera/log.h"

namespace syscalls::internal {
    namespace {
        i64 openat_impl(const char* user_path, u32 flags, mode_t mode, const char* base_path) {
            if (!user_path || user_path[0] == '\0') return -EINVAL;

            const Unit* current_unit = kernel::scheduling::get_current_unit();
            if (!current_unit) return -EINVAL;

            Realm* realm = kernel::scheduling::get_current_realm();
            if (!realm) return -ESRCH;

            char norm[256];
            if (base_path != nullptr) {
                SYSCALL_TRY_VOID(VFS::resolve_path_at(base_path, user_path, norm, sizeof(norm)));
            } else {
                SYSCALL_TRY_VOID(VFS::resolve_path(user_path, norm, sizeof(norm)));
            }

            auto node_res = VFS::open(norm);

            if (node_res.is_err()) {
                if (flags & O_CREAT) {
                    // TODO add umask for realm
                    const auto create_mode = mode & 07777;
                    SYSCALL_TRY_VOID(VFS::create(norm, create_mode));
                    node_res = VFS::open(norm);
                    if (node_res.is_err()) return -ENOENT;
                } else {
                    return node_res.to_errno();
                }
            } else {
                if ((flags & O_CREAT) && (flags & O_EXCL)) {
                    VFS::close(node_res.unwrap());
                    return -EEXIST;
                }
            }

            VfsNode* node = node_res.unwrap();

            u32 vfs_access = 0;
            capability_set required_caps = CAP_NONE;

            switch (flags & 0x3) {
                case O_RDONLY:
                    vfs_access = kernel::security::VFS_ACCESS_READ;
                    required_caps = CAP_READ;
                    break;
                case O_WRONLY:
                    vfs_access = kernel::security::VFS_ACCESS_WRITE;
                    required_caps = CAP_WRITE;
                    break;
                case O_RDWR:
                    vfs_access = kernel::security::VFS_ACCESS_READ | kernel::security::VFS_ACCESS_WRITE;
                    required_caps = CAP_READ | CAP_WRITE;
                    break;
                default:
                    VFS::close(node);
                    return -EINVAL;
            }

            if (const int err = kernel::security::vfs_check_permission(
                    node, vfs_access, SYSCALL_TRY(kernel::security::current_credentials())
                );
                err != 0) {
                VFS::close(node);
                return err;
            }

            if (node->type == VfsNodeType::Directory) {
                if (!(flags & O_DIRECTORY)) {
                    VFS::close(node);
                    return -EISDIR;
                }
            } else {
                if (flags & O_DIRECTORY) {
                    VFS::close(node);
                    return -ENOTDIR;
                }
            }

            if (node->type == VfsNodeType::Fifo) {
                if (!node->fifo_channel) {
                    node->fifo_channel = Channel::create(4096);
                    if (!node->fifo_channel) return -ENOMEM;
                }
                Channel* ch = node->fifo_channel;

                const int acc_mode   = flags & 0x3; // O_ACCMODE
                const bool want_read  = (acc_mode == O_RDONLY || acc_mode == O_RDWR);
                const bool want_write = (acc_mode == O_WRONLY || acc_mode == O_RDWR);
                const bool is_rdwr    = (acc_mode == O_RDWR);
                const bool nonblock   = (flags & O_NONBLOCK) != 0;

                if (want_write && !is_rdwr && nonblock && !ch->has_readers()) {
                    return -ENXIO;
                }

                if (want_read)  ch->add_reader();
                if (want_write) ch->add_writer();

                if (want_read && !is_rdwr && !nonblock && !ch->has_writers()) {
                    Log::debug("waiting for writer");
                    ch->wait_for_writer(kernel::scheduling::get_current_unit());
                }

                if (want_write && !is_rdwr && !nonblock && !ch->has_readers()) {
                    Log::debug("waiting for reader");
                    ch->wait_for_reader(kernel::scheduling::get_current_unit());
                }
            }

            VfsHandle* vh = nullptr;
            u64 handle_type = 0;

            switch (node->type) {
                case VfsNodeType::Fifo:
                    vh = new VfsHandle(node, flags, required_caps, norm);
                    if (!vh) {
                        bool destroyed = false;
                        if ((flags & 0x3) == O_RDONLY || (flags & 0x3) == O_RDWR) destroyed = node->fifo_channel->remove_reader();
                        if (!destroyed && ((flags & 0x3) == O_WRONLY || (flags & 0x3) == O_RDWR)) destroyed = node->fifo_channel->remove_writer();
                        if (destroyed) node->fifo_channel = nullptr;
                        VFS::close(node);
                        return -ENOMEM;
                    }
                    handle_type = HANDLE_TYPE_FIFO;
                    break;

                case VfsNodeType::CharDevice:
                case VfsNodeType::BlockDevice:
                    required_caps |= CAP_DEVICE_ACCESS;
                    vh = new VfsHandle(node, flags, required_caps, norm);
                    if (!vh) {
                        VFS::close(node);
                        return -ENOMEM;
                    }
                    handle_type = HANDLE_TYPE_DEVICE;
                    break;

                case VfsNodeType::File:
                    if (flags & O_TRUNC) {
                        auto trunc_res = VFS::truncate(node, 0);
                        if (trunc_res.is_err()) {
                            VFS::close(node);
                            return trunc_res.to_errno();
                        }
                    }
                    vh = new VfsHandle(node, flags, required_caps, norm);
                    if (!vh) {
                        VFS::close(node);
                        return -ENOMEM;
                    }
                    handle_type = HANDLE_TYPE_FILE;
                    break;

                case VfsNodeType::Directory: {
                    auto dir_res = VFS::opendir(node);
                    if (dir_res.is_err()) {
                        VFS::close(node);
                        return dir_res.to_errno();
                    }
                    VfsDir* dir_handle = dir_res.unwrap();

                    vh = new VfsHandle(node, flags, required_caps, norm);
                    if (!vh) {
                        VFS::closedir(dir_handle);
                        VFS::close(node);
                        return -ENOMEM;
                    }
                    vh->context->type_specific_data = dir_handle;
                    handle_type = HANDLE_TYPE_DIRECTORY;
                    break;
                }

                case VfsNodeType::OtherDevice:
                    required_caps |= CAP_DEVICE_ACCESS;
                    vh = new VfsHandle(node, flags, required_caps, norm);
                    if (!vh) {
                        VFS::close(node);
                        return -ENOMEM;
                    }
                    handle_type = HANDLE_TYPE_DEVICE;
                    break;

                default:
                    VFS::close(node);
                    return -EINVAL;
            }

            if (const capability_set caps = kernel::scheduling::get_current_capabilities();
                (caps & required_caps) != required_caps) {
                VfsHandle::destroy(vh);
                VFS::close(node);
                return -EACCES;
            }

            if ((flags & O_APPEND) && node->type == VfsNodeType::File) vh->context->position = node->size;

            const Result<HandleId> result =
                kernel::realm::add_handle_to_current(handle_type, vh, required_caps, true, VfsHandle::destroy, VfsHandle::acquire);

            if (result.is_err()) {
                if (node->type == VfsNodeType::Directory && vh->node->internal_data && node->ops && node->ops->closedir)
                    VFS::closedir(static_cast<VfsDir*>(vh->node->internal_data));
                VfsHandle::destroy(vh);
                return result.to_errno();
            }

            return static_cast<i64>(result.unwrap());
        }
    }  // namespace

    i64 sys_open(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const auto user_path = reinterpret_cast<const char*>(arg0);
        const auto flags = static_cast<u32>(arg1);
        const auto mode = static_cast<mode_t>(arg2);

        return openat_impl(user_path, flags, mode, nullptr);
    }

    i64 sys_openat(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64, u64) {
        const auto dirfd = static_cast<int>(arg0);
        const auto user_path = reinterpret_cast<const char*>(arg1);
        const auto flags = static_cast<u32>(arg2);
        const auto mode = static_cast<mode_t>(arg3);

        if (dirfd == AT_FDCWD) return openat_impl(user_path, flags, mode, nullptr);

        // Absolute paths ignore dirfd entirely
        if (user_path && user_path[0] == '/') return openat_impl(user_path, flags, mode, nullptr);

        const auto rh = SYSCALL_TRY(resolve_handle(static_cast<HandleId>(dirfd), HANDLE_TYPE_DIRECTORY, CAP_READ));

        const auto* dir_handle = rh.resource_as<VfsHandle>();
        if (!dir_handle || !dir_handle->node || !dir_handle->context) return -EBADH;
        if (dir_handle->node->type != VfsNodeType::Directory) return -ENOTDIR;
        if (dir_handle->context->path[0] == '\0') return -EBADH;

        return openat_impl(user_path, flags, mode, dir_handle->context->path);
    }
}  // namespace syscalls::internal