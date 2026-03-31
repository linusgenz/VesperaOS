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

/**
 * @brief Node type constants for vespera_stat::node_type
 */
#define VSTAT_TYPE_UNKNOWN   0
#define VSTAT_TYPE_FILE      1
#define VSTAT_TYPE_DIR       2
#define VSTAT_TYPE_CHARDEV   3
#define VSTAT_TYPE_BLOCKDEV  4
#define VSTAT_TYPE_SYMLINK   5

/**
 * @brief Flags for vespera_stat::flags
 */
#define VSTAT_FLAG_READABLE  0x01
#define VSTAT_FLAG_WRITABLE  0x02
#define VSTAT_FLAG_EXEC      0x04
#define VSTAT_FLAG_VIRTUAL   0x08  ///< Node lives in a virtual/pseudo FS (devfs, realmfs)
#define VSTAT_FLAG_PERMANENT 0x10  ///< Node is permanent (cannot be unlinked)

/**
 * @brief File/node metadata returned by sys_stat().
 *
 * VesperaOS uses capabilities, not permission bits.
 */
typedef struct vespera_stat {
    u8  node_type;    ///< VSTAT_TYPE_*
    u8  _pad0[3];
    u32 flags;        ///< VSTAT_FLAG_* bitmask
    u32 dev_id;       ///< Kernel device ID (0 for virtual nodes)
    u32 block_size;   ///< Block size of the underlying device (0 if not applicable)
    u64 inode_id;     ///< Filesystem-assigned inode/cluster number (0 if none)
    u64 size;         ///< File size in bytes (0 for directories/devices)
    u64 blocks;       ///< Number of 512-byte blocks allocated (0 if not applicable)
    u32 atime;       ///< Last access time (Unix seconds)
    u32 mtime;       ///< Last modification time (Unix seconds)
    u32 ctime;       ///< Last status change time (Unix seconds)
    u32 crtime;      ///< Creation time (Unix seconds)
    u16 mode;   ///< Raw Unix permission bits (0644 etc.), (0 if not applicable)
    u16 links_count; ///< Hard link count
    u32 uid;         ///< Owner UID (0 if not applicable)
    u32 gid;         ///< Owner GID (0 if not applicable)
} vespera_stat_t;

#endif  // VESPERAOS_STAT_H
