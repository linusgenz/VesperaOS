// ext4.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 20.08.25.
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

#ifndef VESPERAOS_EXT4_H
#define VESPERAOS_EXT4_H

#include "../../kernel/devices/blockdevice.h"
#include <cstdint>
#include <vector.h>

namespace EXT4 {
#define EXT4_MAGIC 0xEF53
#define EXT4_EXTENT_MAGIC 0xF30A
#define READ_DIR_MAX_ENTRIES  256

#define EXT4_S_IFMT   0xF000  // Mask for the file type bits

#define EXT4_S_IFSOCK 0xC000  // Socket
#define EXT4_S_IFLNK  0xA000  // Symbolic link
#define EXT4_S_IFREG  0x8000  // Regular file
#define EXT4_S_IFBLK  0x6000  // Block device
#define EXT4_S_IFDIR  0x4000  // Directory
#define EXT4_S_IFCHR  0x2000  // Character device
#define EXT4_S_IFIFO  0x1000  // FIFO (pipe)


    struct Ext4Superblock {
        uint32_t s_inodes_count;
        uint32_t s_blocks_count_lo;
        uint32_t s_r_blocks_count_lo;
        uint32_t s_free_blocks_count_lo;
        uint32_t s_free_inodes_count;
        uint32_t s_first_data_block;
        uint32_t s_log_block_size;
        uint32_t s_log_cluster_size;
        uint32_t s_blocks_per_group;
        uint32_t s_clusters_per_group;
        uint32_t s_inodes_per_group;
        uint32_t s_mtime;
        uint32_t s_wtime;
        uint16_t s_mnt_count;
        uint16_t s_max_mnt_count;
        uint16_t s_magic;
        uint16_t s_state;
        uint16_t s_errors;
        uint16_t s_minor_rev_level;
        uint32_t s_lastcheck;
        uint32_t s_checkinterval;
        uint32_t s_creator_os;
        uint32_t s_rev_level;
        uint16_t s_def_resuid;
        uint16_t s_def_resgid;

        // EXT4 spezifisch
        uint32_t s_first_ino;
        uint16_t s_inode_size;
        uint16_t s_block_group_nr;
        uint32_t s_feature_compat;
        uint32_t s_feature_incompat;
        uint32_t s_feature_ro_compat;
        uint8_t s_uuid[16];
        uint8_t s_volume_name[16];
        uint8_t s_last_mounted[64];
    } __attribute__((packed));

    struct GroupDesc {
        uint32_t bg_block_bitmap_lo;
        uint32_t bg_inode_bitmap_lo;
        uint32_t bg_inode_table_lo;
        uint16_t bg_free_blocks_count_lo;
        uint16_t bg_free_inodes_count_lo;
        uint16_t bg_used_dirs_count_lo;
        uint16_t bg_flags;
        uint32_t bg_exclude_bitmap_lo;
        uint16_t bg_block_bitmap_csum_lo;
        uint16_t bg_inode_bitmap_csum_lo;
        uint16_t bg_itable_unused_lo;
        uint16_t bg_checksum;
    } __attribute__((packed));


    // on-disk inode (base portion - we only read relevant fields)
    struct Inode {
        uint16_t i_mode; // File mode (type + perms)
        uint16_t i_uid; // Low 16 bits of Owner UID
        uint32_t i_size_lo; // Lower 32 bits of size in bytes
        uint32_t i_atime; // Last access time (UNIX epoch)
        uint32_t i_ctime; // Creation/inode change time
        uint32_t i_mtime; // Last modification time
        uint32_t i_dtime; // Deletion time
        uint16_t i_gid; // Low 16 bits of Group ID
        uint16_t i_links_count; // Hard link count
        uint32_t i_blocks_lo; // Lower 32 bits: # of 512-byte blocks allocated
        uint32_t i_flags; // File flags
        uint32_t i_osd1; // OS-dependent value

        uint32_t i_block[15]; // Pointers to blocks / extents tree root

        uint32_t i_generation; // File version (for NFS)
        uint32_t i_file_acl_lo; // Lower 32 bits of extended attributes block
        uint32_t i_size_high; // Upper 32 bits of file size (since ext4)
        uint32_t i_obso_faddr; // Obsoleted fragment address

        uint16_t i_blocks_high; // Upper 16 bits of i_blocks
        uint16_t i_file_acl_high; // Upper 16 bits of i_file_acl
        uint16_t i_uid_high; // Upper 16 bits of UID
        uint16_t i_gid_high; // Upper 16 bits of GID
        uint16_t i_checksum_lo; // Lower 16 bits of inode checksum
        uint16_t i_reserved; // Padding / reserved

        // If inode size > 128, extra fields follow (common: 256 bytes in ext4):

        uint16_t i_extra_isize; // Size of this extra inode area
        uint16_t i_checksum_hi; // Upper 16 bits of inode checksum
        uint32_t i_ctime_extra; // Extra change time (nsec + epoch)
        uint32_t i_mtime_extra; // Extra modification time
        uint32_t i_atime_extra; // Extra access time
        uint32_t i_crtime; // File creation time (low 32 bits)
        uint32_t i_crtime_extra; // Extra creation time (nsec + epoch)
        uint32_t i_version_hi; // High 32 bits for NFS version
    } __attribute__((packed));

    // directory entry (ext4_dir_entry_2)
    struct DirEntry {
        uint32_t inode;
        uint16_t rec_len;
        uint8_t name_len;
        uint8_t file_type;
        char name[];
    } __attribute__((packed));

    // extent structures
    struct ExtentHeader {
        uint16_t eh_magic; // 0xF30A
        uint16_t eh_entries;
        uint16_t eh_max;
        uint16_t eh_depth;
        uint32_t eh_generation;
    } __attribute__((packed));

    struct Extent {
        uint32_t ee_block; // first logical block extent covers
        uint16_t ee_len; // number of blocks (if high bit set => unwritten)
        uint16_t ee_start_hi;
        uint32_t ee_start_lo;
    } __attribute__((packed));

    struct ExtentIdx {
        uint32_t ei_block;
        uint32_t ei_leaf_lo;
        uint16_t ei_leaf_hi;
        uint16_t ei_unused;
    } __attribute__((packed));

    struct Ext4ExtentMap {
        uint32_t length; // how many logical blocks
        uint64_t phys_start; // phys startblock
    };

    class FileEntry {
        char name[256] = {};
        uint32_t inodeNumber = 0;
        uint8_t type = 0; // 1=File, 2=Dir, 7=Symlink usw. (EXT4_FT_xx)
        bool _isDir = false;

    public:
        void SetName(const char *n, size_t len) {
            if (len >= sizeof(name)) len = sizeof(name) - 1;
            memcpy(name, n, len);
            name[len] = '\0';
        }

        void SetInode(uint32_t ino) { inodeNumber = ino; }

        void SetType(uint8_t t) {
            type = t;
            _isDir = (t == 2); // EXT4_FT_DIR
        }

        const char *GetName() const { return name; }
        uint32_t GetInode() const { return inodeNumber; }
        bool isDir() const { return _isDir; }
        uint8_t GetType() const { return type; }
    };


    class FileSystem {
    public:
        explicit FileSystem(BlockDevice *device);


        bool is_valid() const { return valid; }

        FileEntry *read_directory(uint32_t inodeNumber, size_t &outCount);

        ~FileSystem();

    private:
        BlockDevice *device;
        Ext4Superblock superblock;
        uint32_t sectorSize;
        bool valid;

        [[nodiscard]] uint32_t get_block_size() const {
            return 1024U << superblock.s_log_block_size;
        }

        static uint64_t inode_get_size(const Inode& ino) {
            return (static_cast<uint64_t>(ino.i_size_high) << 32) | ino.i_size_lo;
        }

        static uint64_t inode_get_blocks(const Inode& ino) {
            return (static_cast<uint64_t>(ino.i_blocks_high) << 32) | ino.i_blocks_lo;
        }

        bool read_superblock();

        bool read_block(uint64_t block, void *outBuf);

        bool read_group_desc(uint32_t group, GroupDesc &gd);

        bool read_inode(uint32_t inode_no, Inode &outInode);

        bool parse_extents_from_inode(Inode &inode, Vector<Ext4ExtentMap> &outExtents);

        bool map_logical_to_physical(Inode &inode, uint32_t lblock, uint64_t &out_pblock);
    };
}

#endif //VESPERAOS_EXT4_H
