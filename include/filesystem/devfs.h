// devfs.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 12.09.25.
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

#ifndef VESPERAOS_DEVFS_H
#define VESPERAOS_DEVFS_H

#include <filesystem/virtual_fs.h>
#include <vespera/devices/device_manager.h>
#include <klib/result.h>

#define DEVFS_NAME_MAX 64

struct CharFile;
class CharDevice;
struct VfsNode;
struct VfsHandleContext;

struct DevfsEntry : VirtualFsEntry<KernelDevice> {
    u32 rdev_minor = 0;  // assigned once at register_device() time, reused by stat()
};

class DevFs : public VirtualFilesystem<KernelDevice, DevfsEntry> {
   private:
    static const char* bus_to_str(BusType bus);

   public:
    static void init();

    static int register_device(KernelDevice* kd);
    static int unregister_device(KernelDevice* kd);

    static VoidResult open(VfsNode* node, VfsHandleContext* ctx);
    // VFS operations
    static Result<usize> read(const VfsNode* node, usize offset, usize size, void* buffer, VfsHandleContext* ctx);
    static Result<usize> write(VfsNode* node, usize offset, usize size, const void* buffer, VfsHandleContext* ctx);
    static isize ioctl(const VfsNode* node, u32 cmd, void* arg, VfsHandleContext* ctx);

    static void close_session(VfsNode* node, VfsHandleContext* ctx);
    static int poll(const VfsNode* node, VfsHandleContext* ctx);

    static CharFile* get_char_file(VfsHandleContext* ctx);

    static VoidResult stat(const VfsNode* node, struct stat* out);

   private:
    static u32 next_char_minor_;
    static u32 next_block_minor_;
    static u32 next_other_minor_;
};

#endif  // VESPERAOS_DEVFS_H