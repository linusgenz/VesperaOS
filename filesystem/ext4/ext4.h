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

#include <klib/string.h>
#include <klib/vector.h>
#include <vespera/devices/block.h>

#include "klib/result.h"
#include "uapi/vespera/stat.h"

#ifndef EXT4_INODE_CACHE_SIZE
#define EXT4_INODE_CACHE_SIZE 32768
#endif
#ifndef EXT4_BLOCK_CACHE_SIZE
#define EXT4_BLOCK_CACHE_SIZE 16384
#endif

namespace ext4 {
    constexpr u16 EXT4_MAGIC = 0xEF53;
    constexpr u16 EXT4_EXTENT_MAGIC = 0xF30A;
    constexpr u32 EXT4_ROOT_INODE = 2;
    constexpr u32 EXT4_FIRST_INODE = 11;
    constexpr u32 EXT4_MAX_DIR_ENTRIES = 256;
    constexpr u32 SUPERBLOCK_OFFSET = 1024;
    constexpr u32 DEFAULT_INODE_SIZE = 128;
    constexpr u32 EXT4_NAME_LEN = 255;

    // The i_block area is 60 bytes: 12 bytes for the ExtentHeader leave room for
    // (60 - 12) / 12 = 4 Extent entries at depth 0.
    constexpr u16 EXT4_MAX_INLINE_EXTENTS = 4;

    enum class InodeType : u16 {
        Unknown = 0x0000,
        Fifo = 0x1000,
        CharDevice = 0x2000,
        Directory = 0x4000,
        BlockDevice = 0x6000,
        RegularFile = 0x8000,
        SymbolicLink = 0xA000,
        Socket = 0xC000,
        Mask = 0xF000,
    };

    // ext4 directory entry file_type field (EXT4_FT_*)
    enum class DirEntryType : u8 {
        Unknown = 0,
        RegularFile = 1,
        Directory = 2,
        CharDevice = 3,
        BlockDevice = 4,
        Fifo = 5,
        Socket = 6,
        SymbolicLink = 7,
    };

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

    struct Inode {
        u16 i_mode;
        u16 i_uid;
        u32 i_size_lo;
        u32 i_atime;
        u32 i_ctime;
        u32 i_mtime;
        u32 i_dtime;
        u16 i_gid;
        u16 i_links_count;
        u32 i_blocks_lo;
        u32 i_flags;
        u32 i_osd1;
        u32 i_block[15]; // extent tree root or direct block pointers
        u32 i_generation;
        u32 i_file_acl_lo;
        u32 i_size_high;
        u32 i_obso_faddr;
        u16 i_blocks_high;
        u16 i_file_acl_high;
        u16 i_uid_high;
        u16 i_gid_high;
        u16 i_checksum_lo;
        u16 i_reserved;
        // Extended fields (present when inode size > 128, typically 256 bytes in ext4)
        u16 i_extra_isize;
        u16 i_checksum_hi;
        u32 i_ctime_extra;
        u32 i_mtime_extra;
        u32 i_atime_extra;
        u32 i_crtime;
        u32 i_crtime_extra;
        u32 i_version_hi;
    } __attribute__((packed));

    struct DirEntry {
        u32 inode;
        u16 rec_len;
        u8 name_len;
        u8 file_type; // DirEntryType
        char name[];
    } __attribute__((packed));

    struct ExtentHeader {
        u16 eh_magic; // EXT4_EXTENT_MAGIC
        u16 eh_entries;
        u16 eh_max;
        u16 eh_depth;
        u32 eh_generation;
    } __attribute__((packed));

    struct Extent {
        u32 ee_block; // first logical block this extent covers
        u16 ee_len; // number of blocks (high bit set => unwritten)
        u16 ee_start_hi;
        u32 ee_start_lo;
    } __attribute__((packed));

    struct ExtentIdx {
        u32 ei_block;
        u32 ei_leaf_lo;
        u16 ei_leaf_hi;
        u16 ei_unused;
    } __attribute__((packed));

    struct ExtentMap {
        u32 logical_start; // first logical block
        u32 length; // number of logical blocks
        u64 phys_start; // first physical block
    };

    class FileEntry {
    public:
        FileEntry() = default;

        void set_name(const char* name, usize len) {
            if (len >= sizeof(name_)) len = sizeof(name_) - 1;
            memcpy(name_, name, len);
            name_[len] = '\0';
        }

        void set_inode(const u32 inode) {
            inode_ = inode;
        }

        void set_type(const DirEntryType type) {
            type_ = type;
        }

        void set_type(u8 raw) {
            set_type(static_cast<DirEntryType>(raw));
        }

        [[nodiscard]] const char* get_name() const {
            return name_;
        }

        [[nodiscard]] u32 get_inode() const {
            return inode_;
        }

        [[nodiscard]] DirEntryType get_type() const {
            return type_;
        }

        [[nodiscard]] bool is_dir() const {
            return type_ == DirEntryType::Directory;
        }

        void set_size(u64 sz) {
            size_ = sz;
        }

        [[nodiscard]] u64 get_size() const {
            return size_;
        }

        void set_executable(bool exec) {
            executable_ = exec;
        }

        [[nodiscard]] bool is_executable() const {
            return executable_;
        }

    private:
        char name_[256] = {};
        usize size_ = 0;
        u32 inode_ = 0;
        DirEntryType type_ = DirEntryType::Unknown;
        bool executable_ = false;
    };

    struct InodeCacheEntry {
        u32 inode_no = 0; // 0 → slot is empty
        bool valid = false;
        bool dirty = false;
        u32 lru_seq = 0; // lower = older
        Inode inode = {};
        Vector<ExtentMap> extents;
    };

    struct BlockCacheEntry {
        u64 block_no = 0; // physical block number; 0 → empty (block 0 never cached)
        bool valid = false;
        bool dirty = false;
        u32 lru_seq = 0;
        u8* data = nullptr; // heap-allocated, get_block_size() bytes
    };


    class FileSystem {
    public:
        explicit FileSystem(BlockDevice* device);
        ~FileSystem();
        VoidResult sync() const;
        void flush_inode_cache() const;

        [[nodiscard]] bool is_valid() const {
            return valid_;
        }

        [[nodiscard]] Ext4Superblock* get_superblock() {
            return &superblock_;
        }

        [[nodiscard]] u32 get_block_size() const {
            return 1024u << superblock_.s_log_block_size;
        }

        /**
         * Returns a heap-allocated array of up to EXT4_MAX_DIR_ENTRIES entries.
         * The caller is responsible for freeing the array with kernel::memory::free().
         *If the `find_name` parameter is set, the function returns as soon as a matching entry is found.
         *
         * @warning the size field of all entries is 0, except the entry which matches find_name, if found and specified
         */
        Result<FileEntry*> read_directory(u32 inode_number, usize& out_count, const char* find_name = nullptr) const;
        Result<usize> read_file(u32 inode_number, u64 offset, usize size, void* buf, bool update_atime) const;
        Result<usize> write_file(u32 inode_number, u64 offset, usize size, const void* buf);
        Result<u32> create_file(u32 dir_inode_no, const char* name, mode_t mode);
        Result<u32> create_dir(u32 dir_inode_no, const char* name, mode_t mode);
        VoidResult unlink(u32 dir_inode_no, const char* name);
        VoidResult rmdir(u32 dir_inode_no, const char* name);
        VoidResult rename(u32 old_dir_inode, const char* old_name, u32 new_dir_inode, const char* new_name);
        VoidResult stat(u32 inode_no, stat* out, u32 dev_id) const;
        VoidResult truncate(u32 inode_no, u64 new_size);
        VoidResult chown(u32 inode_no, u32 uid, u32 gid) const;
        VoidResult chmod(u32 inode_no, u16 new_mode) const;

    private:
        BlockDevice* device_;
        Ext4Superblock superblock_{};
        u32 sector_size_;
        bool valid_;

        mutable InodeCacheEntry* inode_cache_{nullptr};
        mutable BlockCacheEntry* block_cache_{nullptr};
        u8* block_data_slab_{nullptr};
        mutable u32 lru_counter_;

        [[nodiscard]] static u64 inode_get_size(const Inode& inode) {
            return (static_cast<u64>(inode.i_size_high) << 32) | inode.i_size_lo;
        }

        static void inode_set_size(Inode& inode, u64 size) {
            inode.i_size_lo = static_cast<u32>(size & 0xFFFFFFFFu);
            inode.i_size_high = static_cast<u32>(size >> 32);
        }

        [[nodiscard]] static InodeType inode_get_type(const Inode& inode) {
            return static_cast<InodeType>(inode.i_mode & static_cast<u16>(InodeType::Mask));
        }

        void flush_block_cache() const;
        bool read_superblock();
        [[nodiscard]] bool write_superblock() const;
        bool read_block_raw(u64 block, void* buf, u32 buf_size) const;
        bool write_block_raw(u64 block, const void* buf, u32 buf_size) const;
        bool read_block(u64 block, void* out_buf, u32 buf_size) const;
        bool write_block(u64 block, const void* buf, u32 buf_size) const;
        bool read_group_desc(u32 group, GroupDesc& out_gd) const;
        [[nodiscard]] bool write_group_desc(u32 group, const GroupDesc& gd) const;
        u64 inode_disk_offset(u32 inode_no, u32& out_inode_size) const;
        bool read_inode_raw(u32 inode_no, Inode& out_inode) const;
        bool write_inode_raw(u32 inode_no, const Inode& inode) const;
        bool read_inode(u32 inode_no, Inode& out_inode) const;
        [[nodiscard]] bool write_inode(u32 inode_no, const Inode& inode) const;
        u32 alloc_inode(u32 preferred_group);
        bool init_inode(u32 inode_no, u16 mode) const;
        bool free_inode(u32 inode_no);
        bool dir_add_entry(u32 dir_inode_no, const char* name, u32 child_inode, DirEntryType type);
        bool dir_remove_entry(u32 dir_inode_no, const char* name) const;
        u32 dir_find_entry(u32 dir_inode_no, const char* name) const;
        [[nodiscard]] bool dir_is_empty(u32 inode_no) const;
        static bool parse_extents_raw(const Inode& inode, Vector<ExtentMap>& out_extents);

        static bool parse_extents(const Inode& inode, Vector<ExtentMap>& out_extents);
        bool map_logical_to_physical(const Inode& inode, u32 lblock, u64& out_pblock) const;
        u64 alloc_block(u64 near_block);
        bool free_block(u64 phys_block);
        bool free_blocks_for_inode(const Inode& inode);
        bool extent_tree_append(Inode& inode, u32 logical_block, u64 phys_block);
    };
} // namespace ext4

#endif  // VESPERAOS_EXT4_H
