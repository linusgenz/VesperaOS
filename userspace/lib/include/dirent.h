// dirent.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 29.09.25.
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

#ifndef VESPERAOS_DIRENT_H
#define VESPERAOS_DIRENT_H

/**
 * @brief Enumeration of possible directory entry types.
 *
 * This enumeration defines the type of an entry returned by `sys_readdir()`
 * or higher-level wrappers (e.g. `readdir()`).
 */
typedef enum {
    DT_UNKNOWN = 0, ///< Unknown entry type (not determined)
    DT_FILE,        ///< Regular file
    DT_DIR,         ///< Directory
    DT_SYMLINK,     ///< Symbolic link
    DT_CHARDEV,     ///< Character device
    DT_BLOCKDEV,    ///< Block device
    DT_FIFO,        ///< Named pipe (FIFO)
    DT_SOCKET,      ///< Socket
    DT_EXEC         ///< Executable file
} dirent_type_t;

/**
 * @brief Structure representing a directory entry.
 *
 * Returned by `sys_readdir()` in user space. Each call fills this structure
 * with the name and type of the next entry in an open directory stream.
 */
typedef struct {
    char name[128];      ///< Null-terminated entry name (up to 127 characters)
    dirent_type_t type;  ///< Type of the directory entry (see ::dirent_type_t)
} dirent_t;

#endif //VESPERAOS_DIRENT_H