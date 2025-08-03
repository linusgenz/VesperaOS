// error_codes.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 03.08.25.
//
// This file is part of LuminOS.
// 
// LuminOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// LuminOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with LuminOS. If not, see <https://www.gnu.org/licenses/>.

#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#define SUCCESS_CODE 0

// Standard POSIX Error Codes
#define EPERM           1   // Operation not permitted
#define ENOENT          2   // No such file or directory
#define ESRCH           3   // No such process
#define EINTR           4   // Interrupted system call
#define EIO             5   // I/O error
#define ENXIO           6   // No such device or address
#define E2BIG           7   // Argument list too long
#define ENOEXEC         8   // Exec format error
#define EBADF           9   // Bad file descriptor
#define ECHILD         10   // No child processes
#define EAGAIN         11   // Try again (resource unavailable)
#define ENOMEM         12   // Out of memory
#define EACCES         13   // Permission denied
#define EFAULT         14   // Bad address (pointer error)
#define EBUSY          16   // Device or resource busy
#define EEXIST         17   // File exists
#define EXDEV          18   // Cross-device link
#define ENODEV         19   // No such device
#define ENOTDIR        20   // Not a directory
#define EISDIR         21   // Is a directory
#define EINVAL         22   // Invalid argument
#define ENFILE         23   // Too many open files in system
#define EMFILE         24   // Too many open files
#define ENOTTY         25   // Inappropriate ioctl for device
#define ETXTBSY        26   // Text file busy
#define EFBIG          27   // File too large
#define ENOSPC         28   // No space left on device
#define ESPIPE         29   // Illegal seek
#define EROFS          30   // Read-only file system
#define EMLINK         31   // Too many links
#define EPIPE          32   // Broken pipe

// Advanced I/O or FS-related
#define EDOM           33   // Math argument out of domain
#define ERANGE         34   // Math result not representable
#define ENAMETOOLONG   36   // File name too long
#define ENOLCK         37   // No record locks available
#define ENOSYS         38   // Function not implemented
#define ENOTEMPTY      39   // Directory not empty
#define ELOOP          40   // Too many symbolic links

// Custom or Extended Kernel/Internal Errors
#define ENOMSG         42   // No message of desired type
#define EOVERFLOW      75   // Value too large
#define EILSEQ         84   // Invalid multibyte sequence

// Nonstandard / OS-specific
#define EUNKNOWN      1000  // Unknown error
#define EUNSUPPORTED  1001  // Operation not supported
#define EDEADLOCK     1002  // Would cause deadlock
#define EWOULDBLOCK   EAGAIN // alias

#endif //ERROR_CODES_H
