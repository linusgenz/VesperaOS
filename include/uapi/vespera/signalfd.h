// signalfd.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 26.09.26.
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
#ifndef VESPERAOS_UAPI_SIGNALFD_H
#define VESPERAOS_UAPI_SIGNALFD_H

#include "types.h"

/* Flags for signalfd.  */
enum
{
    SFD_CLOEXEC = 02000000,
#define SFD_CLOEXEC SFD_CLOEXEC
    SFD_NONBLOCK = 00004000
#define SFD_NONBLOCK SFD_NONBLOCK
  };


struct signalfd_siginfo
{
    u32 ssi_signo;
    i32 ssi_errno;
    i32 ssi_code;
    u32 ssi_pid;
    u32 ssi_uid;
    i32 ssi_fd;
    u32 ssi_tid;
    u32 ssi_band;
    u32 ssi_overrun;
    u32 ssi_trapno;
    i32 ssi_status;
    i32 ssi_int;
    u64 ssi_ptr;
    u64 ssi_utime;
    u64 ssi_stime;
    u64 ssi_addr;
    u16 ssi_addr_lsb;
    u16 __pad2;
    i32 ssi_syscall;
    u64 ssi_call_addr;
    u32 ssi_arch;
    u8 __pad[28];
};

#endif //VESPERAOS_UAPI_SIGNALFD_H
