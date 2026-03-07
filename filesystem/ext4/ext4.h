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

#include <vespera/devices/block.h>
#include <klib/vector.h>

namespace ext4 {
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
        u32 s_inodes_count;
        u32 s_blocks_count_lo;
        u32 s_r_blocks_count_lo;
        u32 s_free_blocks_count_lo;
        u32 s_free_inodes_count;
        u32 s_first_data_block;
        u32 s_log_block_size;
        u32 s_log_cluster_size;
        u32 s_blocks_per_group;
        u32 s_clusters_per_group;
        u32 s_inodes_per_group;
        u32 s_mtime;
        u32 s_wtime;
        u16 s_mnt_count;
        u16 s_max_mnt_count;
        u16 s_magic;
        u16 s_state;
        u16 s_errors;
        u16 s_minor_rev_level;
        u32 s_lastcheck;
        u32 s_checkinterval;
        u32 s_creator_os;
        u32 s_rev_level;
        u16 s_def_resuid;
        u16 s_def_resgid;

        // EXT4 spezifisch
        u32 s_first_ino;
        u16 s_inode_size;
        u16 s_block_group_nr;
        u32 s_feature_compat;
        u32 s_feature_incompat;
        u32 s_feature_ro_compat;
        u8 s_uuid[16];
        u8 s_volume_name[16];
        u8 s_last_mounted[64];
    } __attribute__((packed));

    struct GroupDesc {
        u32 bg_block_bitmap_lo;
        u32 bg_inode_bitmap_lo;
        u32 bg_inode_table_lo;
        u16 bg_free_blocks_count_lo;
        u16 bg_free_inodes_count_lo;
        u16 bg_used_dirs_count_lo;
        u16 bg_flags;
        u32 bg_exclude_bitmap_lo;
        u16 bg_block_bitmap_csum_lo;
        u16 bg_inode_bitmap_csum_lo;
        u16 bg_itable_unused_lo;
        u16 bg_checksum;
    } __attribute__((packed));


    // on-disk inode (base portion - we only read relevant fields)
    struct Inode {
        u16 i_mode; // File mode (type + perms)
        u16 i_uid; // Low 16 bits of Owner UID
        u32 i_size_lo; // Lower 32 bits of size in bytes
        u32 i_atime; // Last access time (UNIX epoch)
        u32 i_ctime; // Creation/inode change time
        u32 i_mtime; // Last modification time
        u32 i_dtime; // Deletion time
        u16 i_gid; // Low 16 bits of Group ID
        u16 i_links_count; // Hard link count
        u32 i_blocks_lo; // Lower 32 bits: # of 512-byte blocks allocated
        u32 i_flags; // File flags
        u32 i_osd1; // OS-dependent value

        u32 i_block[15]; // Pointers to blocks / extents tree root

        u32 i_generation; // File version (for NFS)
        u32 i_file_acl_lo; // Lower 32 bits of extended attributes block
        u32 i_size_high; // Upper 32 bits of file size (since ext4)
        u32 i_obso_faddr; // Obsoleted fragment address

        u16 i_blocks_high; // Upper 16 bits of i_blocks
        u16 i_file_acl_high; // Upper 16 bits of i_file_acl
        u16 i_uid_high; // Upper 16 bits of UID
        u16 i_gid_high; // Upper 16 bits of GID
        u16 i_checksum_lo; // Lower 16 bits of inode checksum
        u16 i_reserved; // Padding / reserved

        // If inode size > 128, extra fields follow (common: 256 bytes in ext4):

        u16 i_extra_isize; // Size of this extra inode area
        u16 i_checksum_hi; // Upper 16 bits of inode checksum
        u32 i_ctime_extra; // Extra change time (nsec + epoch)
        u32 i_mtime_extra; // Extra modification time
        u32 i_atime_extra; // Extra access time
        u32 i_crtime; // File creation time (low 32 bits)
        u32 i_crtime_extra; // Extra creation time (nsec + epoch)
        u32 i_version_hi; // High 32 bits for NFS version
    } __attribute__((packed));

    // directory entry (ext4_dir_entry_2)
    struct DirEntry {
        u32 inode;
        u16 rec_len;
        u8 name_len;
        u8 file_type;
        char name[];
    } __attribute__((packed));

    // extent structures
    struct ExtentHeader {
        u16 eh_magic; // 0xF30A
        u16 eh_entries;
        u16 eh_max;
        u16 eh_depth;
        u32 eh_generation;
    } __attribute__((packed));

    struct Extent {
        u32 ee_block; // first logical block extent covers
        u16 ee_len; // number of blocks (if high bit set => unwritten)
        u16 ee_start_hi;
        u32 ee_start_lo;
    } __attribute__((packed));

    struct ExtentIdx {
        u32 ei_block;
        u32 ei_leaf_lo;
        u16 ei_leaf_hi;
        u16 ei_unused;
    } __attribute__((packed));

    struct Ext4ExtentMap {
        u32 length; // how many logical blocks
        u64 phys_start; // phys startblock
    };

    class FileEntry {
        char name_[256] = {};
        u32 inode_number_ = 0;
        u8 type_ = 0; // 1=File, 2=Dir, 7=Symlink usw. (EXT4_FT_xx)
        bool is_dir_ = false;

    public:
        void set_name(const char *n, usize len) {
            if (len >= sizeof(name_)) len = sizeof(name_) - 1;
            memcpy(name_, n, len);
            name_[len] = '\0';
        }

        void set_inode(u32 ino) { inode_number_ = ino; }

        void set_type(u8 t) {
            type_ = t;
            is_dir_ = (t == 2); // EXT4_FT_DIR
        }

        [[nodiscard]] const char *get_name() const { return name_; }
        [[nodiscard]] u32 get_inode() const { return inode_number_; }
        [[nodiscard]] bool is_dir() const { return is_dir_; }
        [[nodiscard]] u8 get_type() const { return type_; }
    };


    class FileSystem {
    public:
        explicit FileSystem(BlockDevice *device);


        [[nodiscard]] bool is_valid() const { return valid_; }

        FileEntry *read_directory(u32 inode_number, usize &out_count);

        Ext4Superblock* get_superblock();

        ~FileSystem();

    private:
        BlockDevice *device_;
        Ext4Superblock superblock_{};
        u32 sector_size_;
        bool valid_;

        [[nodiscard]] u32 get_block_size() const {
            return 1024U << superblock_.s_log_block_size;
        }

        static u64 inode_get_size(const Inode& ino) {
            return (static_cast<u64>(ino.i_size_high) << 32) | ino.i_size_lo;
        }

        static u64 inode_get_blocks(const Inode& ino) {
            return (static_cast<u64>(ino.i_blocks_high) << 32) | ino.i_blocks_lo;
        }

        bool read_superblock();

        bool read_block(u64 block, void *out_buf) const;

        bool read_group_desc(u32 group, GroupDesc &gd) const;

        bool read_inode(u32 inode_no, Inode &out_inode) const;

        static bool parse_extents_from_inode(Inode &inode, Vector<Ext4ExtentMap> &out_extents);

        bool map_logical_to_physical(Inode &inode, u32 lblock, u64 &out_pblock);
    };
}

#endif //VESPERAOS_EXT4_H
