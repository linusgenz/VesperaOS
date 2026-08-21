// sysmacros.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 21.08.26.
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
#ifndef _SYS_SYSMACROS_H
#define _SYS_SYSMACROS_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline unsigned int gnu_dev_major(unsigned long long int dev) {
    return (unsigned int)(((dev >> 8) & 0xfff) | ((dev >> 32) & ~0xfff));
}

static inline unsigned int gnu_dev_minor(unsigned long long int dev) {
    return (unsigned int)((dev & 0xff) | ((dev >> 12) & ~0xff));
}

static inline unsigned long long int gnu_dev_makedev(unsigned int major, unsigned int minor) {
    return ((unsigned long long int)(minor & 0xff)) |
           ((unsigned long long int)(major & 0xfff) << 8) |
           ((unsigned long long int)(minor & ~0xff) << 12) |
           ((unsigned long long int)(major & ~0xfff) << 32);
}

#define major(dev)       gnu_dev_major((unsigned long long int)(dev))
#define minor(dev)       gnu_dev_minor((unsigned long long int)(dev))
#define makedev(maj, min) gnu_dev_makedev((unsigned int)(maj), (unsigned int)(min))

#ifdef __cplusplus
}
#endif

#endif //_SYS_SYSMACROS_H
