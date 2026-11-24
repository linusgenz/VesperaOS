// ucred.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 25.09.26.
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

#ifndef _SYS_UCRED_H
#define _SYS_UCRED_H

#include <sys/types.h>

#define XU_NGROUPS 16

struct xucred {
    unsigned int cr_version;
    uid_t cr_uid;
    gid_t cr_gid;
    short cr_ngroups;
    gid_t cr_groups[XU_NGROUPS];
    pid_t cr_pid;
};

#define XUCRED_VERSION 0

#endif