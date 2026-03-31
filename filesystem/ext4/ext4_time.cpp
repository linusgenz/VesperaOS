// ext4_time.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 26.03.26.
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

#include <vespera/types.h>
#include <vespera/time.h>
#include <klib/time.h>
#include "ext4.h"

namespace ext4::time {
    static u64 rtc_to_unix_time() {
        u8 sec = 0, min = 0, hour = 0, day = 0, month = 0, year = 0;
        kernel::time::read_rtc(sec, min, hour, day, month, year);
        return klib::time::to_unix(2000u + year, month, day, hour, min, sec);
    }

    void update_write(Inode& inode) {
        const u32 t = static_cast<u32>(rtc_to_unix_time());
        inode.i_mtime = t;
        inode.i_ctime = t;
    }

    void set_creation(Inode& inode) {
        const u32 t = static_cast<u32>(rtc_to_unix_time());
        inode.i_mtime = t;
        inode.i_ctime = t;
        inode.i_atime = t;
        inode.i_crtime =t;
    }

    void update_access(Inode& inode) {
        inode.i_atime = static_cast<u32>(rtc_to_unix_time());
    }
}