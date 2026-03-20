// sys_mount.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 20.03.26.
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

#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>

#include "../../../filesystem/vfs/fs_registry.h"
#include "../../units/unit.h"

static BlockDevice* find_block_device_by_name(const char* name) {
    auto devices = DeviceManager::query([](const KernelDevice* kd) { return kd->block != nullptr; });

    // normalize path
    if (name[0] == '/') {
        name++;
    }
    if (strncmp(name, "dev/", 4) == 0) {
        name += 4;
    }

    for (auto* kd : devices) {
        if (!kd || !kd->name) continue;

        if (strcmp(kd->name, name) == 0) return kd->block;
    }

    return nullptr;
}

namespace syscalls::internal {
    i64 sys_mount(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64, u64) {
        const auto source = reinterpret_cast<const char*>(arg0);
        const auto target = reinterpret_cast<const char*>(arg1);
        const auto fstype = reinterpret_cast<const char*>(arg2);
        const u64 flags = arg3;

        if (target[0] != '/') {
            return -EINVAL;
        }

        BlockDevice* device = nullptr;

        if (source) {
            device = find_block_device_by_name(source);
            if (!device) return -ENODEV;
        }

        for (const auto& mp : VFS::get_mount_points_snapshot()) {
            if (mp->device && mp->device->device == device) {
                return -EBUSY;
            }
        }

        return FilesystemDetector::mount_manual(device, target, fstype ? fstype : nullptr);
    }
}  // namespace syscalls::internal