// sys_umount.cpp
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

#include <vespera_errno.h>

#include "../filesystem/vfs/fs_detection.h" // todo
#include <filesystem/vfs.h>

namespace syscalls::internal {
    i64 sys_umount(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const auto target = reinterpret_cast<const char*>(arg0);

        if (!target) return -EINVAL;

        if (target[0] != '/') {
            return -EINVAL;
        }

        MountPoint* mp = VFS::find_mount_point(target);
        if (!mp) return -ENOENT;
        if (mp->is_root_device || mp->is_virtual) return -EACCES;

        if (!FilesystemDetector::unmount(mp))
            return -EBUSY;

        VFS::remove_mount_point(mp);
        delete mp;

        return 0;
    }
}