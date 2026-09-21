// vfs_handle.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 21.09.25.
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

#ifndef VESPERAOS_VFS_HANDLE_H
#define VESPERAOS_VFS_HANDLE_H

#include <vespera/types.h>
#include "klib/string.h"
#include <filesystem/vfs.h>
#include <filesystem/vfs_node.h>
#include <uapi/vespera/capabilities.h>
#include <uapi/vespera/fcntl.h>

struct VfsHandleContext {
    u32 open_flags; // O_RDONLY, O_WRONLY, O_RDWR
    usize position; // used for offset
    capability_set required_caps;
    /// Per-session state, meaning depends on node->type -- there is
    /// exactly one live interpretation at a time, discriminated the same
    /// way ~VfsHandle() below already discriminates it:
    ///   - Directory:  VfsDir*   (from VFS::opendir(), see sys_open.cpp)
    ///   - CharDevice/
    ///     BlockDevice: CharFile* (from DevFs::open(), see devfs.cpp) --
    ///                  one per open() call now, not one per VfsNode; this
    ///                  field existing per-VfsHandleContext (not per-node)
    ///                  is what fixes two opens of the same device path
    ///                  silently sharing one CharFile*.
    ///   - everything else: unused, stays nullptr.
    void* type_specific_data;
    char path[256]{}; // fully resolved path this handle was opened with; required for openat()
};

struct VfsHandle {
    VfsNode* node;
    VfsHandleContext* context;
    int refcount{1};

    VfsHandle(VfsNode* n, u32 flags, capability_set caps, const char* resolved_path = nullptr)
        : node(n), context(new VfsHandleContext()) {
        context->open_flags = flags;
        context->position = 0;
        context->required_caps = caps;
        context->type_specific_data = nullptr;
        if (resolved_path) {
            strncpy(context->path, resolved_path, sizeof(context->path) - 1);
            context->path[sizeof(context->path) - 1] = '\0';
        }
    }

    static void destroy(void* ptr) {
        auto* vh = static_cast<VfsHandle*>(ptr);
        if (!vh) return;
        if (__sync_sub_and_fetch(&vh->refcount, 1) != 0) return;
        delete vh;
    }

    static void acquire(void* ptr) {
        auto* vh = static_cast<VfsHandle*>(ptr);
        if (!vh) return;
        __sync_add_and_fetch(&vh->refcount, 1);
    }

private:
    ~VfsHandle() {
        if (node) {
            if (node->type == VfsNodeType::Fifo && node->fifo_channel && context) {
                Channel* ch = node->fifo_channel;
                const u32 acc = context->open_flags & 0x3;
                bool destroyed = false;

                if (acc == O_RDONLY || acc == O_RDWR) destroyed = ch->remove_reader();
                if (!destroyed && (acc == O_WRONLY || acc == O_RDWR)) destroyed = ch->remove_writer();

                if (destroyed) node->fifo_channel = nullptr;
            }


            if (node->type == VfsNodeType::Directory &&
                context && context->type_specific_data) {
                VFS::closedir(static_cast<VfsDir*>(context->type_specific_data));
                context->type_specific_data = nullptr;
            }

            //   1. close_session -- always runs, ends *this* session
            //      (releases context->type_specific_data for CharDevice/
            //      BlockDevice, i.e. DevFs's per-open() CharFile*).
            //   2. close -- only runs when `node`'s dentry-cache refcount
            //      reaches zero, i.e. only once every session (including
            //      this one) is gone. This is where a real filesystem
            //      would free the inode's backing structure etc.
            VFS::close_session(node, context);
            VFS::close(node);
        }
        delete context;
    }
};

#endif //VESPERAOS_VFS_HANDLE_H
