// stat.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.03.26.
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
#ifndef VESPERAOS_STAT_H
#define VESPERAOS_STAT_H

#include <vespera/types.h>
#include "time.h"

/**
 * @brief Node type constants for vespera_stat::node_type
 */
#define VSTAT_TYPE_UNKNOWN   0
#define VSTAT_TYPE_FILE      1
#define VSTAT_TYPE_DIR       2
#define VSTAT_TYPE_CHARDEV   4
#define VSTAT_TYPE_BLOCKDEV  5
#define VSTAT_TYPE_SYMLINK   3

/**
 * @brief Flags for vespera_stat::flags
 */
#define VSTAT_FLAG_READABLE  0x01
#define VSTAT_FLAG_WRITABLE  0x02
#define VSTAT_FLAG_EXEC      0x04
#define VSTAT_FLAG_VIRTUAL   0x08  ///< Node lives in a virtual/pseudo FS (devfs, realmfs)
#define VSTAT_FLAG_PERMANENT 0x10  ///< Node is permanent (cannot be unlinked)


#define S_IFMT   0170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000

/* -------------------------------------------------------------------------- */
/* POSIX S_IS* Macros                                                         */
/* -------------------------------------------------------------------------- */
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

/* -------------------------------------------------------------------------- */
/* POSIX Permissions Flags (octal)                                            */
/* -------------------------------------------------------------------------- */
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000

#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001


/**
 * @brief File/node metadata returned by sys_stat().
 */
struct stat {
    dev_t     st_dev;       ///< ID of device containing file
    ino_t     st_ino;       ///< Inode number
    mode_t    st_mode;      ///< File type and Mode (permissions)
    nlink_t   st_nlink;     ///< Number of hard links
    uid_t     st_uid;       ///< User ID of owner
    gid_t     st_gid;       ///< Group ID of owner
    dev_t     st_rdev;      ///< Device ID (if special file)
    off_t     st_size;      ///< Total size, in bytes
    blksize_t st_blksize;   ///< Block size for filesystem I/O
    blkcnt_t  st_blocks;    ///< Number of 512B blocks allocated

    struct timespec st_atim;  ///< Time of last access
    struct timespec st_mtim;  ///< Time of last modification
    struct timespec st_ctim;  ///< Time of last status change

    /* VesperaOS-spezifische Erweiterungen (am Ende platziert, um POSIX nicht zu stören) */
    u8   v_node_type;  ///< VSTAT_TYPE_*
    u8   _v_pad[3];
    u32  v_flags;      ///< VSTAT_FLAG_* bitmask
    u32  v_crtime;     ///< Creation time (Unix seconds)
};

#endif  // VESPERAOS_STAT_H
