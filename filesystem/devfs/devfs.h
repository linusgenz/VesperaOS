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

#include <vespera/devices/device_manager.h>

#include "../virtual_fs.h"

#define DEVFS_NAME_MAX 64

struct CharFile;
class CharDevice;

typedef int (*dev_open_t)(CharFile** out_cf);

typedef int (*dev_release_t)(CharFile* cf);

typedef size_t (*dev_read_t)(CharFile* cf, void* buf, size_t count);  // non-positional
typedef size_t (*dev_write_t)(CharFile* cf, const void* buf, size_t count);

typedef int (*dev_ioctl_t)(CharFile* cf, unsigned long req, void* arg);

typedef int (*dev_poll_t)(CharFile* cf);  // returns POLLIN/POLLOUT mask-ish

// handle for device drivers

struct DevfsEntry : VirtualFsEntry<KernelDevice> {
    CharFile* cf;
};

class DevFs : public VirtualFilesystem<KernelDevice, DevfsEntry> {
   private:
    static const char* bus_to_str(BusType bus);

   public:
    static void init();

    static int register_device(KernelDevice* kd);
    static int unregister_device(KernelDevice* kd);

    static int open(const VfsNode* node);
    // VFS operations
    static ssize_t read(const VfsNode* node, size_t offset, size_t size, void* buffer);
    static ssize_t write(VfsNode* node, size_t offset, size_t size, const void* buffer);
    static ssize_t ioctl(const VfsNode* node, uint32_t cmd, void* arg);
    static void close(VfsNode* node);
};

#endif  // VESPERAOS_DEVFS_H
