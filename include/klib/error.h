// error.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 30.04.26.
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

#ifndef VESPERAOS_VESPERA_KERRNO_H
#define VESPERAOS_VESPERA_KERRNO_H

#include <vespera/types.h>

namespace klib {

    enum class Error : i32 {
        SUCCESS = 0,  // No error

        // POSIX-compatible codes (match vespera_errno.h exactly)
        EPERM = 1,          // Operation not permitted
        ENOENT = 2,         // No such file or directory
        ESRCH = 3,          // No such process
        EINTR = 4,          // Interrupted system call
        EIO = 5,            // I/O error
        ENXIO = 6,          // No such device or address
        E2BIG = 7,          // Argument list too long
        ENOEXEC = 8,        // Exec format error
        EBADH = 9,          // Bad handle id
        ECHILD = 10,        // No child processes
        EAGAIN = 11,        // Try again (resource unavailable)
        ENOMEM = 12,        // Out of memory
        EACCES = 13,        // Permission denied
        EFAULT = 14,        // Bad address (pointer error)
        EBUSY = 16,         // Device or resource busy
        EEXIST = 17,        // File exists
        EXDEV = 18,         // Cross-device link
        ENODEV = 19,        // No such device
        ENOTDIR = 20,       // Not a directory
        EISDIR = 21,        // Is a directory
        EINVAL = 22,        // Invalid argument
        ENFILE = 23,        // Too many open files in system
        EMFILE = 24,        // Too many open files
        ENOTTY = 25,        // Inappropriate ioctl for device
        ETXTBSY = 26,       // Text file busy
        EFBIG = 27,         // File too large
        ENOSPC = 28,        // No space left on device
        ESPIPE = 29,        // Illegal seek
        EROFS = 30,         // Read-only file system
        EMLINK = 31,        // Too many links
        EPIPE = 32,         // Broken pipe
        EDOM = 33,          // Math argument out of domain
        ERANGE = 34,        // Math result not representable
        ENAMETOOLONG = 36,  // File name too long
        ENOLCK = 37,        // No record locks available
        ENOSYS = 38,        // Function not implemented
        ENOTEMPTY = 39,     // Directory not empty
        ELOOP = 40,         // Too many symbolic links
        ENOMSG = 42,        // No message of desired type
        EOVERFLOW = 75,     // Value too large
        EILSEQ = 84,        // Invalid multibyte sequence

        EUNKNOWN = 1000,      // Unknown error
        EUNSUPPORTED = 1001,  // Operation not supported
        EDEADLOCK = 1002,     // Would cause deadlock
    };
}  // namespace klib

using klib::Error;


#endif  // VESPERAOS_VESPERA_KERRNO_H
