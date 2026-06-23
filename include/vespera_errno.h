// vespera_errno.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 03.08.25.
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

#ifndef ERROR_CODES_H
#define ERROR_CODES_H

inline constexpr int SUCCESS_CODE = 0;

inline constexpr int EPERM  = 1;
inline constexpr int ENOENT = 2;
inline constexpr int ESRCH  = 3;
inline constexpr int EINTR  = 4;
inline constexpr int EIO    = 5;
inline constexpr int ENXIO  = 6;
inline constexpr int E2BIG  = 7;
inline constexpr int ENOEXEC= 8;
inline constexpr int EBADH  = 9;
inline constexpr int ECHILD = 10;
inline constexpr int EAGAIN = 11;
inline constexpr int ENOMEM = 12;
inline constexpr int EACCES = 13;
inline constexpr int EFAULT = 14;
inline constexpr int EBUSY  = 16;
inline constexpr int EEXIST = 17;
inline constexpr int EXDEV  = 18;
inline constexpr int ENODEV = 19;
inline constexpr int ENOTDIR= 20;
inline constexpr int EISDIR = 21;
inline constexpr int EINVAL = 22;
inline constexpr int ENFILE = 23;
inline constexpr int EMFILE = 24;
inline constexpr int ENOTTY = 25;
inline constexpr int ETXTBSY= 26;
inline constexpr int EFBIG  = 27;
inline constexpr int ENOSPC = 28;
inline constexpr int ESPIPE = 29;
inline constexpr int EROFS  = 30;
inline constexpr int EMLINK = 31;
inline constexpr int EPIPE  = 32;

inline constexpr int EDOM         = 33;
inline constexpr int ERANGE       = 34;
inline constexpr int ENAMETOOLONG = 36;
inline constexpr int ENOLCK       = 37;
inline constexpr int ENOSYS       = 38;
inline constexpr int ENOTEMPTY    = 39;
inline constexpr int ELOOP        = 40;

inline constexpr int ENOMSG    = 42;
inline constexpr int EOVERFLOW = 75;
inline constexpr int EILSEQ    = 84;

inline constexpr int ETIMEDOUT = 116; /* Connection timed out */

inline constexpr int EUNKNOWN     = 1000;
inline constexpr int EUNSUPPORTED = 1001;
inline constexpr int EDEADLOCK    = 1002;

// Alias
inline constexpr int EWOULDBLOCK = EAGAIN;

#endif //ERROR_CODES_H
