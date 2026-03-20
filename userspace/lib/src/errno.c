// errno.c
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

#include <errno.h>

const char* strerror(int err) {
    switch (-err) {
        case SUCCESS_CODE: return "Success";
        case EPERM:        return "Operation not permitted";
        case ENOENT:       return "No such file or directory";
        case ESRCH:        return "No such process";
        case EINTR:        return "Interrupted system call";
        case EIO:          return "I/O error";
        case ENXIO:        return "No such device or address";
        case E2BIG:        return "Argument list too long";
        case ENOEXEC:      return "Exec format error";
        case EBADH:        return "Bad handle id";
        case ECHILD:       return "No child processes";
        case EAGAIN:       return "Resource temporarily unavailable";
        case ENOMEM:       return "Out of memory";
        case EACCES:       return "Permission denied";
        case EFAULT:       return "Bad address";
        case EBUSY:        return "Device or resource busy";
        case EEXIST:       return "File exists";
        case EXDEV:        return "Cross-device link";
        case ENODEV:       return "No such device";
        case ENOTDIR:      return "Not a directory";
        case EISDIR:       return "Is a directory";
        case EINVAL:       return "Invalid argument";
        case ENFILE:       return "Too many open files in system";
        case EMFILE:       return "Too many open files";
        case ENOTTY:       return "Inappropriate ioctl for device";
        case ETXTBSY:      return "Text file busy";
        case EFBIG:        return "File too large";
        case ENOSPC:       return "No space left on device";
        case ESPIPE:       return "Illegal seek";
        case EROFS:        return "Read-only file system";
        case EMLINK:       return "Too many links";
        case EPIPE:        return "Broken pipe";
        case ENAMETOOLONG: return "File name too long";
        case ENOLCK:       return "No locks available";
        case ENOSYS:       return "Function not implemented";
        case ENOTEMPTY:    return "Directory not empty";
        case ELOOP:        return "Too many symbolic links";
        case EOVERFLOW:    return "Value too large";
        case EILSEQ:       return "Invalid byte sequence";
        case EUNKNOWN:     return "Unknown error";
        case EUNSUPPORTED: return "Operation not supported";
        case EDEADLOCK:    return "Deadlock detected";
        default:           return "Unknown error";
    }
}