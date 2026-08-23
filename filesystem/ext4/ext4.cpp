// ext4.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 01.08.25.
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

#include "ext4.h"

#include <klib/string.h>
#include <klib/vector.h>
#include <vespera/mm/memory.h>
#include <vespera/security/credentials.h>
#include <uapi/vespera/time.h>

#include "ext4_time.h"
#include "klib/result.h"
#include "uapi/vespera/stat.h"

constexpr usize bytes_to_pages(usize bytes) {
    return (bytes + 4095) / 4096;
}

namespace ext4 {
    FileSystem::FileSystem(BlockDevice* device)
        : device_(device)
          , sector_size_(device->get_sector_size())
          , valid_(false)
          , lru_counter_(0) {
        constexpr usize inode_cache_pages = bytes_to_pages(sizeof(InodeCacheEntry) * EXT4_INODE_CACHE_SIZE);
        constexpr usize block_cache_pages = bytes_to_pages(sizeof(BlockCacheEntry) * EXT4_BLOCK_CACHE_SIZE);

        inode_cache_ = static_cast<InodeCacheEntry*>(virt_ptr(kernel::memory::request_pages(inode_cache_pages)));
        block_cache_ = static_cast<BlockCacheEntry*>(virt_ptr(kernel::memory::request_pages(block_cache_pages)));

        if (inode_cache_) {
            for (u32 i = 0; i < EXT4_INODE_CACHE_SIZE; ++i) {
                new(&inode_cache_[i]) InodeCacheEntry();
            }
        }
        if (block_cache_) {
            memset(block_cache_, 0, sizeof(BlockCacheEntry) * EXT4_BLOCK_CACHE_SIZE);
        }

        valid_ = read_superblock();

        if (valid_) {
            const u32 bsize = get_block_size();
            const usize slab_pages = bytes_to_pages(static_cast<usize>(EXT4_BLOCK_CACHE_SIZE) * bsize);
            block_data_slab_ = static_cast<u8*>(virt_ptr(kernel::memory::request_pages(slab_pages)));

            if (block_data_slab_) {
                memset(block_data_slab_, 0, static_cast<usize>(EXT4_BLOCK_CACHE_SIZE) * bsize);
                for (u32 i = 0; i < EXT4_BLOCK_CACHE_SIZE; ++i) {
                    block_cache_[i].data = block_data_slab_ + static_cast<usize>(i) * bsize;
                    block_cache_[i].valid = false;
                    block_cache_[i].dirty = false;
                    block_cache_[i].block_no = 0;
                    block_cache_[i].lru_seq = 0;
                }
            }
        }
    }

    FileSystem::~FileSystem() {
        sync();

        if (block_cache_) {
            constexpr usize block_cache_pages = bytes_to_pages(sizeof(BlockCacheEntry) * EXT4_BLOCK_CACHE_SIZE);
            kernel::memory::free_pages(make_virt(block_cache_), block_cache_pages);
            block_cache_ = nullptr;
        }

        if (block_data_slab_) {
            const u32 bsize = get_block_size();
            const usize slab_pages = bytes_to_pages(static_cast<usize>(EXT4_BLOCK_CACHE_SIZE) * bsize);
            kernel::memory::free_pages(make_virt(block_data_slab_), slab_pages);
            block_data_slab_ = nullptr;
        }

        if (inode_cache_) {
            for (u32 i = 0; i < EXT4_INODE_CACHE_SIZE; ++i) {
                inode_cache_[i].~InodeCacheEntry();
            }
            constexpr usize inode_cache_pages = bytes_to_pages(sizeof(InodeCacheEntry) * EXT4_INODE_CACHE_SIZE);
            kernel::memory::free_pages(make_virt(inode_cache_), inode_cache_pages);
            inode_cache_ = nullptr;
        }
    }

    VoidResult FileSystem::sync() const {
        flush_inode_cache();
        flush_block_cache();
        return VoidResult::ok();
    }

    void FileSystem::flush_inode_cache() const {
        for (u32 i = 0; i < EXT4_INODE_CACHE_SIZE; ++i) {
            if (inode_cache_[i].valid && inode_cache_[i].dirty) {
                write_inode_raw(inode_cache_[i].inode_no, inode_cache_[i].inode);
                inode_cache_[i].dirty = false;
            }
        }
    }

    void FileSystem::flush_block_cache() const {
        for (u32 i = 0; i < EXT4_BLOCK_CACHE_SIZE; ++i) {
            if (block_cache_[i].valid && block_cache_[i].dirty) {
                write_block_raw(block_cache_[i].block_no, block_cache_[i].data, get_block_size());
                block_cache_[i].dirty = false;
            }
        }
    }

    // superblock

    bool FileSystem::read_superblock() {
        const u32 start_sector = SUPERBLOCK_OFFSET / sector_size_;
        const u32 sector_count = (sizeof(Ext4Superblock) + sector_size_ - 1) / sector_size_;
        const u32 buf_size = sector_count * sector_size_;

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return false;

        const bool ok = device_->read(start_sector, sector_count, buf, buf_size);
        if (ok) memcpy(&superblock_, buf, sizeof(Ext4Superblock));
        kernel::memory::free(buf);

        if (!ok) return false;

        if (superblock_.s_magic != EXT4_MAGIC) {
            return false;
        }

        return true;
    }

    bool FileSystem::write_superblock() const {
        const u32 start_sector = SUPERBLOCK_OFFSET / sector_size_;
        const u32 sector_count = (sizeof(Ext4Superblock) + sector_size_ - 1) / sector_size_;
        const u32 buf_size = sector_count * sector_size_;

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return false;

        bool ok = device_->read(start_sector, sector_count, buf, buf_size);
        if (ok) {
            memcpy(buf + (SUPERBLOCK_OFFSET % sector_size_), &superblock_, sizeof(Ext4Superblock));
            ok = device_->write(start_sector, sector_count, buf, buf_size);
        }

        kernel::memory::free(buf);
        return ok;
    }

    // block io (Cached & Raw)

    bool FileSystem::read_block_raw(u64 block, void* buf, u32 buf_size) const {
        const u64 bsize = get_block_size();
        const u64 start_byte = block * bsize;
        const u64 start_sector = start_byte / sector_size_;
        const u32 count = (bsize + sector_size_ - 1) / sector_size_;
        return device_->read(start_sector, count, buf, buf_size);
    }

    bool FileSystem::write_block_raw(u64 block, const void* buf, u32 buf_size) const {
        const u64 bsize = get_block_size();
        const u64 start_byte = block * bsize;
        const u64 start_sector = start_byte / sector_size_;
        const u32 count = (bsize + sector_size_ - 1) / sector_size_;
        return device_->write(start_sector, count, buf, buf_size);
    }

    bool FileSystem::read_block(u64 block, void* buf, u32 buf_size) const {
        // 1. Suche im Block-Cache
        for (u32 i = 0; i < EXT4_BLOCK_CACHE_SIZE; ++i) {
            if (block_cache_[i].valid && block_cache_[i].block_no == block) {
                block_cache_[i].lru_seq = ++lru_counter_;
                const u32 copy_size = (buf_size < get_block_size()) ? buf_size : get_block_size();
                memcpy(buf, block_cache_[i].data, copy_size);
                return true;
            }
        }

        // 2. Cache-Miss: Finde freien Slot oder evictiere ältesten (LRU)
        u32 victim_idx = 0;
        u32 min_lru = 0xFFFFFFFF;
        bool found_empty = false;

        for (u32 i = 0; i < EXT4_BLOCK_CACHE_SIZE; ++i) {
            if (!block_cache_[i].valid) {
                victim_idx = i;
                found_empty = true;
                break;
            }
            if (block_cache_[i].lru_seq < min_lru) {
                min_lru = block_cache_[i].lru_seq;
                victim_idx = i;
            }
        }

        if (!found_empty && block_cache_[victim_idx].dirty) {
            if (!write_block_raw(block_cache_[victim_idx].block_no, block_cache_[victim_idx].data, get_block_size())) {
                return false;
            }
            block_cache_[victim_idx].dirty = false;
        }

        if (!read_block_raw(block, block_cache_[victim_idx].data, get_block_size())) {
            return false;
        }

        block_cache_[victim_idx].block_no = block;
        block_cache_[victim_idx].valid = true;
        block_cache_[victim_idx].dirty = false;
        block_cache_[victim_idx].lru_seq = ++lru_counter_;

        const u32 copy_size = (buf_size < get_block_size()) ? buf_size : get_block_size();
        memcpy(buf, block_cache_[victim_idx].data, copy_size);
        return true;
    }

    bool FileSystem::write_block(u64 block, const void* buf, u32 buf_size) const {
        // 1. Suche im Block-Cache
        for (u32 i = 0; i < EXT4_BLOCK_CACHE_SIZE; ++i) {
            if (block_cache_[i].valid && block_cache_[i].block_no == block) {
                block_cache_[i].lru_seq = ++lru_counter_;
                const u32 copy_size = (buf_size < get_block_size()) ? buf_size : get_block_size();
                memcpy(block_cache_[i].data, buf, copy_size);
                block_cache_[i].dirty = true;
                return true;
            }
        }

        // 2. Cache-Miss: Finde freien Slot oder evictiere ältesten (LRU)
        u32 victim_idx = 0;
        u32 min_lru = 0xFFFFFFFF;
        bool found_empty = false;

        for (u32 i = 0; i < EXT4_BLOCK_CACHE_SIZE; ++i) {
            if (!block_cache_[i].valid) {
                victim_idx = i;
                found_empty = true;
                break;
            }
            if (block_cache_[i].lru_seq < min_lru) {
                min_lru = block_cache_[i].lru_seq;
                victim_idx = i;
            }
        }

        // Falls Slot dirty ist, wegschreiben
        if (!found_empty && block_cache_[victim_idx].dirty) {
            if (!write_block_raw(block_cache_[victim_idx].block_no, block_cache_[victim_idx].data, get_block_size())) {
                return false;
            }
            block_cache_[victim_idx].dirty = false;
        }

        // Bei einem unvollständigen Block-Schreibvorgang lesen wir zuerst den alten Block ein
        if (buf_size < get_block_size()) {
            if (!read_block_raw(block, block_cache_[victim_idx].data, get_block_size())) {
                return false;
            }
        }

        const u32 copy_size = (buf_size < get_block_size()) ? buf_size : get_block_size();
        memcpy(block_cache_[victim_idx].data, buf, copy_size);
        block_cache_[victim_idx].block_no = block;
        block_cache_[victim_idx].valid = true;
        block_cache_[victim_idx].dirty = true;
        block_cache_[victim_idx].lru_seq = ++lru_counter_;

        return true;
    }

    // group descriptor

    // Actual on-disk size of a single group descriptor.
    u32 FileSystem::group_desc_stride() const {
        if ((superblock_.s_feature_incompat & EXT4_INCOMPAT_64BIT) && superblock_.s_desc_size != 0) {
            return superblock_.s_desc_size;
        }
        return 32;
    }

    static u64 group_desc_offset(u32 group, u32 bsize, u32 desc_stride) {
        const u64 gd_table_block = (bsize == 1024) ? 2 : 1;
        return gd_table_block * bsize + static_cast<u64>(group) * desc_stride;
    }

    bool FileSystem::read_group_desc(u32 group, GroupDesc& out_gd) const {
        const u32 bsize = get_block_size();
        const u32 desc_stride = group_desc_stride();
        const u64 gd_offset = group_desc_offset(group, bsize, desc_stride);
        const u64 start_sect = gd_offset / sector_size_;
        const u32 count = (desc_stride + sector_size_ - 1) / sector_size_;
        const u32 buf_size = count * sector_size_;

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return false;

        const bool ok = device_->read(start_sect, count, buf, buf_size);
        if (ok) {
            out_gd = GroupDesc{};
            // Only copy as many bytes as actually exist on disk for this
            // descriptor (32 on legacy filesystems, 64 with the 64bit
            // feature) - never read past what's really there.
            const u32 copy_size = desc_stride < sizeof(GroupDesc) ? desc_stride : sizeof(GroupDesc);
            memcpy(&out_gd, buf + (gd_offset % sector_size_), copy_size);
        }

        kernel::memory::free(buf);
        return ok;
    }

    bool FileSystem::write_group_desc(u32 group, const GroupDesc& gd) const {
        const u32 bsize = get_block_size();
        const u32 desc_stride = group_desc_stride();
        const u64 gd_offset = group_desc_offset(group, bsize, desc_stride);
        const u64 start_sect = gd_offset / sector_size_;
        const u32 count = (desc_stride + sector_size_ - 1) / sector_size_;
        const u32 buf_size = count * sector_size_;

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return false;

        // Read-modify-write um benachbarte Desktriptoren nicht zu beschädigen.
        bool ok = device_->read(start_sect, count, buf, buf_size);
        if (ok) {
            const u32 copy_size = desc_stride < sizeof(GroupDesc) ? desc_stride : sizeof(GroupDesc);
            memcpy(buf + (gd_offset % sector_size_), &gd, copy_size);
            ok = device_->write(start_sect, count, buf, buf_size);
        }

        kernel::memory::free(buf);
        return ok;
    }

    // inode (Cached & Raw)

    u64 FileSystem::inode_disk_offset(u32 inode_no, u32& out_inode_size) const {
        if (inode_no == 0) return 0;

        const u32 inodes_per_group = superblock_.s_inodes_per_group;
        const u32 group = (inode_no - 1) / inodes_per_group;
        const u32 index = (inode_no - 1) % inodes_per_group;

        GroupDesc gd{};
        if (!read_group_desc(group, gd)) return 0;

        u64 inode_table_block = gd.bg_inode_table_lo;
        if (inode_table_block == 0) {
            inode_table_block = (get_block_size() == 1024) ? 5 : 1;
        }

        out_inode_size = (superblock_.s_inode_size == 0) ? DEFAULT_INODE_SIZE : superblock_.s_inode_size;
        return inode_table_block * get_block_size() + static_cast<u64>(index) * out_inode_size;
    }

    bool FileSystem::read_inode_raw(u32 inode_no, Inode& out_inode) const {
        if (inode_no == 0) return false;

        u32 inode_size = 0;
        const u64 inode_offset = inode_disk_offset(inode_no, inode_size);
        if (inode_offset == 0) return false;

        const u64 start_sector = inode_offset / sector_size_;
        const u32 count = (inode_size + sector_size_ - 1) / sector_size_;
        const u32 buf_size = count * sector_size_;

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return false;

        const bool ok = device_->read(start_sector, count, buf, buf_size);
        if (ok) {
            memcpy(&out_inode, buf + (inode_offset % sector_size_), sizeof(Inode));
        }

        kernel::memory::free(buf);
        return ok;
    }

    bool FileSystem::write_inode_raw(u32 inode_no, const Inode& inode) const {
        u32 inode_size = 0;
        const u64 inode_offset = inode_disk_offset(inode_no, inode_size);
        if (inode_offset == 0) return false;

        const u64 start_sector = inode_offset / sector_size_;
        const u32 count = (inode_size + sector_size_ - 1) / sector_size_;
        const u32 buf_size = count * sector_size_;

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return false;

        // Read-modify-write um zusätzliche Inode-Felder zu erhalten.
        bool ok = device_->read(start_sector, count, buf, buf_size);
        if (ok) {
            memcpy(buf + (inode_offset % sector_size_), &inode, sizeof(Inode));
            ok = device_->write(start_sector, count, buf, buf_size);
        }

        kernel::memory::free(buf);
        return ok;
    }

    bool FileSystem::read_inode(u32 inode_no, Inode& out_inode) const {
        if (inode_no == 0) return false;

        // 1. Suche im Inode-Cache
        for (u32 i = 0; i < EXT4_INODE_CACHE_SIZE; ++i) {
            if (inode_cache_[i].valid && inode_cache_[i].inode_no == inode_no) {
                inode_cache_[i].lru_seq = ++lru_counter_;
                out_inode = inode_cache_[i].inode;
                return true;
            }
        }

        // 2. Cache-Miss: Finde freien Slot oder evictiere ältesten (LRU)
        u32 victim_idx = 0;
        u32 min_lru = 0xFFFFFFFF;
        bool found_empty = false;

        for (u32 i = 0; i < EXT4_INODE_CACHE_SIZE; ++i) {
            if (!inode_cache_[i].valid) {
                victim_idx = i;
                found_empty = true;
                break;
            }
            if (inode_cache_[i].lru_seq < min_lru) {
                min_lru = inode_cache_[i].lru_seq;
                victim_idx = i;
            }
        }

        // Wenn der evictierte Slot dirty ist, schreiben wir ihn auf Platte zurück
        if (!found_empty && inode_cache_[victim_idx].dirty) {
            if (!write_inode_raw(inode_cache_[victim_idx].inode_no, inode_cache_[victim_idx].inode)) {
                return false;
            }
            inode_cache_[victim_idx].dirty = false;
        }

        // Raw-Lesevorgang von Platte
        Inode disk_inode{};
        if (!read_inode_raw(inode_no, disk_inode)) {
            return false;
        }

        // Cache füllen
        inode_cache_[victim_idx].inode_no = inode_no;
        inode_cache_[victim_idx].inode = disk_inode;
        inode_cache_[victim_idx].valid = true;
        inode_cache_[victim_idx].dirty = false;
        inode_cache_[victim_idx].lru_seq = ++lru_counter_;

        // Extent-Cache parallel füllen
        inode_cache_[victim_idx].extents.clear();
        parse_extents_raw(disk_inode, inode_cache_[victim_idx].extents);

        out_inode = disk_inode;
        return true;
    }

    bool FileSystem::write_inode(u32 inode_no, const Inode& inode) const {
        if (inode_no == 0) return false;

        // 1. Suche im Inode-Cache und aktualisiere ihn (Lazy atime profitiert hier direkt!)
        for (u32 i = 0; i < EXT4_INODE_CACHE_SIZE; ++i) {
            if (inode_cache_[i].valid && inode_cache_[i].inode_no == inode_no) {
                inode_cache_[i].inode = inode;
                inode_cache_[i].dirty = true;
                inode_cache_[i].lru_seq = ++lru_counter_;

                // Extent-Cache aktualisieren, da sich Inode geändert haben könnte
                inode_cache_[i].extents.clear();
                parse_extents_raw(inode, inode_cache_[i].extents);
                return true;
            }
        }

        // 2. Cache-Miss: LRU-Verdrängung
        u32 victim_idx = 0;
        u32 min_lru = 0xFFFFFFFF;
        bool found_empty = false;

        for (u32 i = 0; i < EXT4_INODE_CACHE_SIZE; ++i) {
            if (!inode_cache_[i].valid) {
                victim_idx = i;
                found_empty = true;
                break;
            }
            if (inode_cache_[i].lru_seq < min_lru) {
                min_lru = inode_cache_[i].lru_seq;
                victim_idx = i;
            }
        }

        // Falls dirty wegschreiben
        if (!found_empty && inode_cache_[victim_idx].dirty) {
            if (!write_inode_raw(inode_cache_[victim_idx].inode_no, inode_cache_[victim_idx].inode)) {
                return false;
            }
            inode_cache_[victim_idx].dirty = false;
        }

        inode_cache_[victim_idx].inode_no = inode_no;
        inode_cache_[victim_idx].inode = inode;
        inode_cache_[victim_idx].valid = true;
        inode_cache_[victim_idx].dirty = true; // Später wegschreiben (Write-Back)
        inode_cache_[victim_idx].lru_seq = ++lru_counter_;

        inode_cache_[victim_idx].extents.clear();
        parse_extents_raw(inode, inode_cache_[victim_idx].extents);

        return true;
    }

    u32 FileSystem::alloc_inode(u32 preferred_group) {
        const u32 bsize = get_block_size();
        const u32 inodes_per_group = superblock_.s_inodes_per_group;
        const u32 total_groups = (superblock_.s_inodes_count + inodes_per_group - 1) / inodes_per_group;

        auto* bitmap = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!bitmap) return 0;

        for (u32 pass = 0; pass < total_groups; ++pass) {
            const u32 group = (preferred_group + pass) % total_groups;

            GroupDesc gd{};
            if (!read_group_desc(group, gd)) continue;
            if (gd.bg_free_inodes_count_lo == 0) continue;
            if (!read_block(gd.bg_inode_bitmap_lo, bitmap, bsize)) continue;

            const u32 inodes_in_group = (group == total_groups - 1)
                                            ? ((superblock_.s_inodes_count - 1) % inodes_per_group + 1)
                                            : inodes_per_group;

            for (u32 byte = 0; byte < (inodes_in_group + 7) / 8; ++byte) {
                if (bitmap[byte] == 0xFF) continue;

                for (u32 bit = 0; bit < 8; ++bit) {
                    if (bitmap[byte] & (1u << bit)) continue;

                    const u32 inode_no = group * inodes_per_group + byte * 8 + bit + 1;
                    if (inode_no < EXT4_FIRST_INODE) continue; // Reservierte Inodes

                    bitmap[byte] |= (1u << bit);
                    if (!write_block(gd.bg_inode_bitmap_lo, bitmap, bsize)) {
                        kernel::memory::free(bitmap);
                        return 0;
                    }

                    gd.bg_free_inodes_count_lo--;
                    write_group_desc(group, gd);

                    superblock_.s_free_inodes_count--;
                    write_superblock();

                    kernel::memory::free(bitmap);
                    return inode_no;
                }
            }
        }

        kernel::memory::free(bitmap);
        return 0;
    }

    bool FileSystem::init_inode(u32 inode_no, u16 mode) const {
        Inode inode{};
        memset(&inode, 0, sizeof(Inode));

        inode.i_mode = mode;
        inode.i_links_count = 1;
        time::set_creation(inode);

        auto cred = kernel::security::current_credentials();
        if (!cred) return false;

        inode.i_uid = static_cast<u16>(cred->uid);
        inode.i_gid = static_cast<u16>(cred->gid);
        inode.i_uid_high = static_cast<u16>(cred->uid >> 16);
        inode.i_gid_high = static_cast<u16>(cred->gid >> 16);

        auto* eh = reinterpret_cast<ExtentHeader*>(&inode.i_block[0]);
        eh->eh_magic = EXT4_EXTENT_MAGIC;
        eh->eh_entries = 0;
        eh->eh_max = EXT4_MAX_INLINE_EXTENTS;
        eh->eh_depth = 0;
        eh->eh_generation = 0;

        inode.i_flags |= 0x00080000u; // EXT4_EXTENTS_FL

        return write_inode(inode_no, inode);
    }

    bool FileSystem::free_inode(u32 inode_no) {
        if (inode_no == 0) return false;

        // Ungültig machen im Inode-Cache falls vorhanden
        for (u32 i = 0; i < EXT4_INODE_CACHE_SIZE; ++i) {
            if (inode_cache_[i].valid && inode_cache_[i].inode_no == inode_no) {
                inode_cache_[i].valid = false;
                inode_cache_[i].dirty = false;
                inode_cache_[i].extents.clear();
            }
        }

        const u32 bsize = get_block_size();
        const u32 inodes_per_group = superblock_.s_inodes_per_group;
        const u32 group = (inode_no - 1) / inodes_per_group;
        const u32 index = (inode_no - 1) % inodes_per_group;

        GroupDesc gd{};
        if (!read_group_desc(group, gd)) return false;

        auto* bitmap = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!bitmap) return false;

        if (!read_block(gd.bg_inode_bitmap_lo, bitmap, bsize)) {
            kernel::memory::free(bitmap);
            return false;
        }

        const u32 byte = index / 8;
        const u32 bit = index % 8;

        if (!(bitmap[byte] & (1u << bit))) {
            kernel::memory::free(bitmap);
            return true; // Bereits frei
        }

        bitmap[byte] &= ~(1u << bit);

        if (!write_block(gd.bg_inode_bitmap_lo, bitmap, bsize)) {
            kernel::memory::free(bitmap);
            return false;
        }

        kernel::memory::free(bitmap);

        gd.bg_free_inodes_count_lo++;
        write_group_desc(group, gd);

        superblock_.s_free_inodes_count++;
        write_superblock();

        return true;
    }

    // directory entry insert

    bool FileSystem::dir_add_entry(u32 dir_inode_no, const char* name, u32 child_inode, DirEntryType type) {
        const u32 bsize = get_block_size();
        const u8 name_len = static_cast<u8>(strlen(name));
        const u16 needed = static_cast<u16>((sizeof(DirEntry) + name_len + 3u) & ~3u);

        Inode dir_inode{};
        if (!read_inode(dir_inode_no, dir_inode)) return false;

        auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!block_buf) return false;

        const u64 dir_size = inode_get_size(dir_inode);
        const u32 block_count = static_cast<u32>((dir_size + bsize - 1) / bsize);

        // Pass 1: Suche nach Slack-Space in bestehendem Verzeichnis-Block
        for (u32 lblock = 0; lblock < block_count; ++lblock) {
            u64 pblock = 0;
            if (!map_logical_to_physical(dir_inode, lblock, pblock)) continue;
            if (!read_block(pblock, block_buf, bsize)) continue;

            usize offset = 0;
            while (offset + sizeof(DirEntry) <= bsize) {
                auto* de = reinterpret_cast<DirEntry*>(block_buf + offset);
                if (de->rec_len == 0) break;

                const u16 de_min = static_cast<u16>((sizeof(DirEntry) + de->name_len + 3u) & ~3u);
                const u16 slack = de->rec_len - de_min;

                if (de->inode == 0) {
                    if (de->rec_len >= needed) {
                        de->inode = child_inode;
                        de->file_type = static_cast<u8>(type);
                        de->name_len = name_len;
                        memcpy(de->name, name, name_len);

                        bool ok = write_block(pblock, block_buf, bsize);
                        kernel::memory::free(block_buf);
                        return ok;
                    }
                } else if (slack >= needed) {
                    const u16 old_rec = de->rec_len;
                    de->rec_len = de_min;

                    auto* ne = reinterpret_cast<DirEntry*>(block_buf + offset + de_min);
                    ne->inode = child_inode;
                    ne->rec_len = old_rec - de_min;
                    ne->name_len = name_len;
                    ne->file_type = static_cast<u8>(type);
                    memcpy(ne->name, name, name_len);

                    bool ok = write_block(pblock, block_buf, bsize);
                    kernel::memory::free(block_buf);
                    return ok;
                }

                offset += de->rec_len;
            }
        }

        // Pass 2: Kein Slack-Space gefunden -> neuen Verzeichnis-Block allokieren
        const u64 new_pblock = alloc_block(0);
        if (new_pblock == 0) {
            kernel::memory::free(block_buf);
            return false;
        }

        memset(block_buf, 0, bsize);
        auto* de = reinterpret_cast<DirEntry*>(block_buf);
        de->inode = child_inode;
        de->rec_len = static_cast<u16>(bsize);
        de->name_len = name_len;
        de->file_type = static_cast<u8>(type);
        memcpy(de->name, name, name_len);

        if (!write_block(new_pblock, block_buf, bsize)) {
            kernel::memory::free(block_buf);
            return false;
        }

        kernel::memory::free(block_buf);

        const u32 new_lblock = block_count;
        if (!extent_tree_append(dir_inode, new_lblock, new_pblock)) {
            return false;
        }

        const u64 old_blocks_512 =
            static_cast<u64>(dir_inode.i_blocks_lo) | (static_cast<u64>(dir_inode.i_blocks_high) << 32);
        const u64 new_blocks_512 = old_blocks_512 + bsize / 512;
        dir_inode.i_blocks_lo = static_cast<u32>(new_blocks_512 & 0xFFFFFFFFu);
        dir_inode.i_blocks_high = static_cast<u16>(new_blocks_512 >> 32);

        inode_set_size(dir_inode, dir_size + bsize);

        return write_inode(dir_inode_no, dir_inode);
    }

    u32 FileSystem::dir_find_entry(const u32 dir_inode_no, const char* name) const {
        const auto name_len = static_cast<u8>(strlen(name));

        Inode dir_inode{};
        if (!read_inode(dir_inode_no, dir_inode)) return 0;

        const u32 bsize = get_block_size();
        const u32 block_count = static_cast<u32>((inode_get_size(dir_inode) + bsize - 1) / bsize);

        auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!block_buf) return 0;

        for (u32 lblock = 0; lblock < block_count; ++lblock) {
            u64 pblock = 0;
            if (!map_logical_to_physical(dir_inode, lblock, pblock)) continue;
            if (!read_block(pblock, block_buf, bsize)) continue;

            usize offset = 0;
            while (offset + sizeof(DirEntry) <= bsize) {
                const auto* de = reinterpret_cast<const DirEntry*>(block_buf + offset);
                if (de->rec_len == 0) break;

                if (de->inode != 0 && de->name_len == name_len && memcmp(de->name, name, name_len) == 0) {
                    const u32 found = de->inode;
                    kernel::memory::free(block_buf);
                    return found;
                }
                offset += de->rec_len;
            }
        }

        kernel::memory::free(block_buf);
        return 0;
    }

    bool FileSystem::dir_remove_entry(u32 dir_inode_no, const char* name) const {
        const u32 bsize = get_block_size();
        const u8 name_len = static_cast<u8>(strlen(name));

        Inode dir_inode{};
        if (!read_inode(dir_inode_no, dir_inode)) return false;

        auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!block_buf) return false;

        const u64 dir_size = inode_get_size(dir_inode);
        const u32 block_count = static_cast<u32>((dir_size + bsize - 1) / bsize);

        for (u32 lblock = 0; lblock < block_count; ++lblock) {
            u64 pblock = 0;
            if (!map_logical_to_physical(dir_inode, lblock, pblock)) continue;
            if (!read_block(pblock, block_buf, bsize)) continue;

            usize offset = 0;
            DirEntry* prev = nullptr;

            while (offset + sizeof(DirEntry) <= bsize) {
                auto* de = reinterpret_cast<DirEntry*>(block_buf + offset);
                if (de->rec_len == 0) break;

                if (de->inode != 0 && de->name_len == name_len && memcmp(de->name, name, name_len) == 0) {
                    if (prev) {
                        prev->rec_len += de->rec_len;
                    } else {
                        de->inode = 0;
                    }

                    bool ok = write_block(pblock, block_buf, bsize);
                    kernel::memory::free(block_buf);
                    return ok;
                }

                prev = de;
                offset += de->rec_len;
            }
        }

        kernel::memory::free(block_buf);
        return false;
    }

    bool FileSystem::dir_is_empty(u32 inode_no) const {
        usize count = 0;
        Result<FileEntry*> entries_result = read_directory(inode_no, count);
        if (entries_result.is_err()) return true;
        FileEntry* entries = entries_result.unwrap();

        bool empty = true;
        for (usize i = 0; i < count; ++i) {
            const char* n = entries[i].name;
            if (strcmp(n, ".") != 0 && strcmp(n, "..") != 0) {
                empty = false;
                break;
            }
        }

        kernel::memory::free(entries);
        return empty;
    }

    // extend tree

    bool FileSystem::parse_extents_raw(const Inode& inode, Vector<ExtentMap>& out_extents) {
        ExtentHeader eh{};
        memcpy(&eh, &inode.i_block[0], sizeof(ExtentHeader));

        if (eh.eh_magic != EXT4_EXTENT_MAGIC) return false;
        if (eh.eh_depth != 0) return false;

        const auto* base = reinterpret_cast<const u8*>(&inode.i_block[0]) + sizeof(ExtentHeader);
        for (u16 i = 0; i < eh.eh_entries; ++i) {
            Extent ex{};
            memcpy(&ex, base + i * sizeof(Extent), sizeof(Extent));

            const u64 phys_start = (static_cast<u64>(ex.ee_start_hi) << 32) | ex.ee_start_lo;
            const u32 len = ex.ee_len & 0x7FFF;

            out_extents.push_back({ex.ee_block, len, phys_start});
        }
        return true;
    }

    bool FileSystem::parse_extents(const Inode& inode, Vector<ExtentMap>& out_extents) {
        return parse_extents_raw(inode, out_extents);
    }

    bool FileSystem::map_logical_to_physical(const Inode& inode, u32 lblock, u64& out_pblock) const {
        // An inode using the extent format stores an ExtentHeader in the first
        // 12 bytes of i_block[], not direct block pointers. If we fall through to
        // treating i_block[lblock] as a raw block number for such an inode (e.g.
        // because its extent list happens to be empty for this lblock), we'd be
        // reinterpreting extent-header/entry bytes as a physical block number —
        // silently corrupting the read/write instead of correctly reporting "no
        // mapping yet". The i_block[] fallback below is only valid for inodes
        // that never used the extent format to begin with.
        ExtentHeader eh_probe{};
        memcpy(&eh_probe, &inode.i_block[0], sizeof(ExtentHeader));
        const bool uses_extents = (eh_probe.eh_magic == EXT4_EXTENT_MAGIC);

        // 1. Versuche die Extents aus dem Inode-Cache direkt abzugreifen
        for (u32 i = 0; i < EXT4_INODE_CACHE_SIZE; ++i) {
            if (inode_cache_[i].valid && memcmp(&inode_cache_[i].inode, &inode, sizeof(Inode)) == 0) {
                // Cache-Hit: Durchsuche den bereits geparsten Extent-Cache
                for (const ExtentMap& ext : inode_cache_[i].extents) {
                    if (lblock >= ext.logical_start && lblock < ext.logical_start + ext.length) {
                        out_pblock = ext.phys_start + (lblock - ext.logical_start);
                        return true;
                    }
                }
                if (!uses_extents && lblock < 12) {
                    const u32 p = inode_cache_[i].inode.i_block[lblock];
                    if (p == 0) return false;
                    out_pblock = p;
                    return true;
                }
                return false;
            }
        }

        // 2. Cache-Miss (Fallback, falls der Inode ausnahmsweise nicht im Cache liegt)
        Vector<ExtentMap> extents;
        if (parse_extents_raw(inode, extents)) {
            for (const ExtentMap& ext : extents) {
                if (lblock >= ext.logical_start && lblock < ext.logical_start + ext.length) {
                    out_pblock = ext.phys_start + (lblock - ext.logical_start);
                    return true;
                }
            }
            return false;
        }

        if (!uses_extents && lblock < 12) {
            const u32 p = inode.i_block[lblock];
            if (p == 0) return false;
            out_pblock = p;
            return true;
        }

        return false;
    }

    // block allocation

    u64 FileSystem::alloc_block(u64 near_block) {
        const u32 bsize = get_block_size();
        const u32 blocks_per_group = superblock_.s_blocks_per_group;
        const u32 total_groups = (superblock_.s_blocks_count_lo + blocks_per_group - 1) / blocks_per_group;

        const u32 preferred_group =
            (near_block > 0) ? static_cast<u32>((near_block - superblock_.s_first_data_block) / blocks_per_group) : 0;

        auto* bitmap = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!bitmap) return 0;

        for (u32 pass = 0; pass < total_groups; ++pass) {
            const u32 group = (preferred_group + pass) % total_groups;

            GroupDesc gd{};
            if (!read_group_desc(group, gd)) continue;
            if (gd.bg_free_blocks_count_lo == 0) continue;

            if (!read_block(gd.bg_block_bitmap_lo, bitmap, bsize)) continue;

            for (u32 byte = 0; byte < blocks_per_group / 8; ++byte) {
                if (bitmap[byte] == 0xFF) continue;

                for (u32 bit = 0; bit < 8; ++bit) {
                    if (bitmap[byte] & (1u << bit)) continue;

                    bitmap[byte] |= (1u << bit);
                    if (!write_block(gd.bg_block_bitmap_lo, bitmap, bsize)) {
                        kernel::memory::free(bitmap);
                        return 0;
                    }

                    gd.bg_free_blocks_count_lo--;
                    write_group_desc(group, gd);

                    superblock_.s_free_blocks_count_lo--;
                    write_superblock();

                    const u64 phys_block = static_cast<u64>(superblock_.s_first_data_block) +
                        static_cast<u64>(group) * blocks_per_group + byte * 8 + bit;

                    kernel::memory::free(bitmap);
                    return phys_block;
                }
            }
        }

        kernel::memory::free(bitmap);
        return 0;
    }

    bool FileSystem::free_block(u64 phys_block) {
        const u32 bsize = get_block_size();
        const u32 blocks_per_group = superblock_.s_blocks_per_group;

        const u64 relative = phys_block - superblock_.s_first_data_block;
        const u32 group = static_cast<u32>(relative / blocks_per_group);
        const u32 index = static_cast<u32>(relative % blocks_per_group);

        GroupDesc gd{};
        if (!read_group_desc(group, gd)) return false;

        auto* bitmap = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!bitmap) return false;

        if (!read_block(gd.bg_block_bitmap_lo, bitmap, bsize)) {
            kernel::memory::free(bitmap);
            return false;
        }

        const u32 byte = index / 8;
        const u32 bit = index % 8;

        if (!(bitmap[byte] & (1u << bit))) {
            kernel::memory::free(bitmap);
            return true;
        }

        bitmap[byte] &= ~(1u << bit);

        if (!write_block(gd.bg_block_bitmap_lo, bitmap, bsize)) {
            kernel::memory::free(bitmap);
            return false;
        }

        kernel::memory::free(bitmap);

        gd.bg_free_blocks_count_lo++;
        write_group_desc(group, gd);

        superblock_.s_free_blocks_count_lo++;
        write_superblock();

        return true;
    }

    bool FileSystem::free_blocks_for_inode(const Inode& inode) {
        Vector<ExtentMap> extents;
        if (!parse_extents_raw(inode, extents)) {
            for (u32 i = 0; i < 12; ++i) {
                if (inode.i_block[i] != 0) free_block(inode.i_block[i]);
            }
            return true;
        }

        for (const ExtentMap& em : extents) {
            for (u32 i = 0; i < em.length; ++i) free_block(em.phys_start + i);
        }
        return true;
    }

    // append new lead extent

    bool FileSystem::extent_tree_append(Inode& inode, u32 logical_block, u64 phys_block) {
        auto* eh = reinterpret_cast<ExtentHeader*>(&inode.i_block[0]);

        if (eh->eh_magic != EXT4_EXTENT_MAGIC) {
            eh->eh_magic = EXT4_EXTENT_MAGIC;
            eh->eh_entries = 0;
            eh->eh_max = EXT4_MAX_INLINE_EXTENTS;
            eh->eh_depth = 0;
            eh->eh_generation = 0;
        }

        if (eh->eh_depth != 0) {
            return false;
        }

        if (eh->eh_entries >= EXT4_MAX_INLINE_EXTENTS) {
            return false;
        }

        if (eh->eh_entries > 0) {
            auto* last = reinterpret_cast<Extent*>(
                reinterpret_cast<u8*>(eh) + sizeof(ExtentHeader) + (eh->eh_entries - 1) * sizeof(Extent)
            );

            const u64 last_phys_end =
                ((static_cast<u64>(last->ee_start_hi) << 32) | last->ee_start_lo) + (last->ee_len & 0x7FFFu);
            const u32 last_log_end = last->ee_block + (last->ee_len & 0x7FFFu);

            if (last_log_end == logical_block && last_phys_end == phys_block && (last->ee_len & 0x7FFFu) < 0x7FFF) {
                last->ee_len++;
                return true;
            }
        }

        auto* ex = reinterpret_cast<Extent*>(
            reinterpret_cast<u8*>(eh) + sizeof(ExtentHeader) + eh->eh_entries * sizeof(Extent)
        );

        ex->ee_block = logical_block;
        ex->ee_len = 1;
        ex->ee_start_hi = static_cast<u16>(phys_block >> 32);
        ex->ee_start_lo = static_cast<u32>(phys_block & 0xFFFFFFFFu);
        eh->eh_entries++;

        return true;
    }

    // fs ops

    Result<FileEntry*> FileSystem::read_directory(
        const u32 inode_number, usize& out_count, const char* find_name
    ) const {
        out_count = 0;

        Inode dir_inode{};
        if (!read_inode(inode_number, dir_inode)) return Result<FileEntry*>::err(Error::Io);

        if (inode_get_type(dir_inode) != InodeType::Directory) return Result<FileEntry*>::err(Error::NotDir);

        auto* entries = static_cast<FileEntry*>(kernel::memory::malloc(sizeof(FileEntry) * EXT4_MAX_DIR_ENTRIES));
        if (!entries) return Result<FileEntry*>::err(Error::NoMem);
        for (usize i = 0; i < EXT4_MAX_DIR_ENTRIES; i++) {
            new(&entries[i]) FileEntry();
        }

        const u32 bsize = get_block_size();
        const u64 dir_size = inode_get_size(dir_inode);
        const u32 block_count = static_cast<u32>((dir_size + bsize - 1) / bsize);

        auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!block_buf) {
            kernel::memory::free(entries);
            return Result<FileEntry*>::err(Error::NoMem);
        }

        for (u32 lblock = 0; lblock < block_count && out_count < EXT4_MAX_DIR_ENTRIES; ++lblock) {
            u64 pblock = 0;
            if (!map_logical_to_physical(dir_inode, lblock, pblock)) continue;
            if (!read_block(pblock, block_buf, bsize)) continue;

            usize offset = 0;
            while (offset + sizeof(DirEntry) <= bsize && out_count < EXT4_MAX_DIR_ENTRIES) {
                const auto* de = reinterpret_cast<const DirEntry*>(block_buf + offset);

                if (de->rec_len == 0) break;
                if (de->inode == 0) {
                    offset += de->rec_len;
                    continue;
                }

                FileEntry& fe = entries[out_count++];
                fe.inode = de->inode;
                fe.type = static_cast<DirEntryType>(de->file_type);
                fe.set_name(de->name, de->name_len);
                fe.size = 0;

                if (find_name && strcmp(fe.name, find_name) == 0) {
                    if (Inode file_inode{}; read_inode(de->inode, file_inode))
                        fe.size = inode_get_size(file_inode);

                    kernel::memory::free(block_buf);
                    return Result<FileEntry*>::ok(entries);
                }

                offset += de->rec_len;
            }
        }

        kernel::memory::free(block_buf);
        return Result<FileEntry*>::ok(entries);
    }

    Result<usize> FileSystem::read_file(u32 inode_number, u64 offset, usize size, void* buf, bool update_atime) const {
        Inode inode{};
        if (!read_inode(inode_number, inode)) return Result<usize>::err(Error::Io);
        if (inode_get_type(inode) != InodeType::RegularFile) return Result<usize>::err(Error::IsDir);

        const u64 file_size = inode_get_size(inode);
        if (offset >= file_size) return Result<usize>::ok(0);
        if (offset + size > file_size) size = static_cast<usize>(file_size - offset);

        const u32 bsize = get_block_size();
        auto* out = static_cast<u8*>(buf);
        usize bytes_read = 0;

        // Hole Extent-Liste aus dem Inode-Cache
        const Vector<ExtentMap>* extents = nullptr;
        for (u32 i = 0; i < EXT4_INODE_CACHE_SIZE; ++i) {
            if (inode_cache_[i].valid && inode_cache_[i].inode_no == inode_number) {
                extents = &inode_cache_[i].extents;
                break;
            }
        }

        // Fallback: kein Extent-Cache → Block-für-Block (alter Pfad)
        if (!extents || extents->empty()) {
            auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
            if (!block_buf) return Result<usize>::err(Error::NoMem);

            usize remaining = size;
            u64 cur_off = offset;
            while (remaining > 0) {
                const u32 lblock = static_cast<u32>(cur_off / bsize);
                const u32 block_off = static_cast<u32>(cur_off % bsize);
                const usize chunk = (remaining < bsize - block_off) ? remaining : (bsize - block_off);
                u64 pblock = 0;
                if (!map_logical_to_physical(inode, lblock, pblock)) {
                    memset(out, 0, chunk);
                } else {
                    if (!read_block(pblock, block_buf, bsize)) {
                        kernel::memory::free(block_buf);
                        return Result<usize>::err(Error::Io);
                    }
                    memcpy(out, block_buf + block_off, chunk);
                }
                out += chunk;
                cur_off += chunk;
                remaining -= chunk;
                bytes_read += chunk;
            }
            kernel::memory::free(block_buf);
            if (update_atime) {
                time::update_access(inode);
                write_inode(inode_number, inode);
            }
            return Result<usize>::ok(bytes_read);
        }

        // Batched-Pfad: iteriere über Extents
        constexpr usize MAX_TRANSFER = 64 * 1024;

        auto* tmp = static_cast<u8*>(kernel::memory::malloc(bsize)); // für unaligned Ränder
        if (!tmp) return Result<usize>::err(Error::NoMem);

        const u64 read_end = offset + size;

        for (usize ei = 0; ei < extents->size() && bytes_read < size; ++ei) {
            const ExtentMap& em = (*extents)[ei];

            const u64 ext_start = static_cast<u64>(em.logical_start) * bsize;
            const u64 ext_end = ext_start + static_cast<u64>(em.length) * bsize;

            // Kein Overlap mit angefordertem Bereich
            if (ext_end <= offset || ext_start >= read_end) continue;

            const u64 overlap_start = (offset > ext_start) ? offset : ext_start;
            const u64 overlap_end = (read_end < ext_end) ? read_end : ext_end;

            u64 cur = overlap_start;
            u8* dst = out + (overlap_start - offset);

            // Leading unaligned section (first block)
            if (cur % bsize != 0) {
                const u32 lb = static_cast<u32>(cur / bsize);
                const u32 block_off = static_cast<u32>(cur % bsize);
                const u64 pb = em.phys_start + (lb - em.logical_start);
                const usize to_copy = (bsize - block_off < static_cast<usize>(overlap_end - cur))
                                          ? bsize - block_off
                                          : static_cast<usize>(overlap_end - cur);

                if (!read_block(pb, tmp, bsize)) {
                    kernel::memory::free(tmp);
                    return Result<usize>::err(Error::Io);
                }
                memcpy(dst, tmp + block_off, to_copy);
                dst += to_copy;
                cur += to_copy;
                bytes_read += to_copy;
            }

            // Aligned middle section: directly via device_->read() in chunks
            {
                const u64 aligned_end = overlap_end & ~static_cast<u64>(bsize - 1);
                const usize full_run_bytes = (aligned_end > cur)
                                                 ? static_cast<usize>(aligned_end - cur)
                                                 : 0;

                if (full_run_bytes > 0) {
                    const u64 phys_block = em.phys_start + (cur / bsize - em.logical_start);
                    const u64 start_sect = phys_block * bsize / sector_size_;
                    const u32 total_sectors = static_cast<u32>(full_run_bytes / sector_size_);

                    // flush dirty cache blocks
                    const u64 run_phys_last = phys_block + (full_run_bytes / bsize) - 1;
                    for (u32 ci = 0; ci < EXT4_BLOCK_CACHE_SIZE; ++ci) {
                        if (!block_cache_[ci].valid) continue;
                        const u64 cb = block_cache_[ci].block_no;
                        if (cb < phys_block || cb > run_phys_last) continue;
                        if (block_cache_[ci].dirty) {
                            write_block_raw(cb, block_cache_[ci].data, bsize);
                            block_cache_[ci].dirty = false;
                        }
                        block_cache_[ci].valid = false;
                    }

                    if (!device_->read(start_sect, total_sectors, dst, full_run_bytes)) {
                        kernel::memory::free(tmp);
                        return Result<usize>::err(Error::Io);
                    }

                    dst += full_run_bytes;
                    cur += full_run_bytes;
                    bytes_read += full_run_bytes;
                }
            }

            // Final unaligned section (last block)
            if (cur < overlap_end) {
                const u32 lb = static_cast<u32>(cur / bsize);
                const u64 pb = em.phys_start + (lb - em.logical_start);
                const usize to_copy = static_cast<usize>(overlap_end - cur);

                if (!read_block(pb, tmp, bsize)) {
                    kernel::memory::free(tmp);
                    return Result<usize>::err(Error::Io);
                }
                memcpy(dst, tmp, to_copy);
                bytes_read += to_copy;
            }
        }

        kernel::memory::free(tmp);

        if (update_atime) {
            time::update_access(inode);
            write_inode(inode_number, inode);
        }
        return Result<usize>::ok(bytes_read);
    }

    Result<usize> FileSystem::write_file(u32 inode_number, u64 offset, usize size, const void* buf) {
        Inode inode{};
        if (!read_inode(inode_number, inode)) return Result<usize>::err(Error::Io);

        if (inode_get_type(inode) != InodeType::RegularFile) return Result<usize>::err(Error::IsDir);

        const u32 bsize = get_block_size();
        auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!block_buf) return Result<usize>::err(Error::NoMem);

        const auto* src = static_cast<const u8*>(buf);
        usize remaining = size;
        u64 cur_off = offset;
        u64 last_phys = 0;

        while (remaining > 0) {
            const u32 lblock = static_cast<u32>(cur_off / bsize);
            const u32 block_off = static_cast<u32>(cur_off % bsize);
            const usize chunk = (remaining < bsize - block_off) ? remaining : (bsize - block_off);

            u64 pblock = 0;

            if (!map_logical_to_physical(inode, lblock, pblock)) {
                pblock = alloc_block(last_phys);
                if (pblock == 0) {
                    kernel::memory::free(block_buf);
                    const usize written = size - remaining;
                    if (written == 0) return Result<usize>::err(Error::NoSpc);
                    write_inode(inode_number, inode);
                    return Result<usize>::ok(written);
                }

                memset(block_buf, 0, bsize);
                if (!write_block(pblock, block_buf, bsize)) {
                    kernel::memory::free(block_buf);
                    return Result<usize>::err(Error::Io);
                }

                if (!extent_tree_append(inode, lblock, pblock)) {
                    kernel::memory::free(block_buf);
                    return Result<usize>::err(Error::NoSpc);
                }

                const u64 blocks_512 =
                    static_cast<u64>(inode.i_blocks_lo) | (static_cast<u64>(inode.i_blocks_high) << 32);
                const u64 new_blocks = blocks_512 + bsize / 512;
                inode.i_blocks_lo = static_cast<u32>(new_blocks & 0xFFFFFFFFu);
                inode.i_blocks_high = static_cast<u16>(new_blocks >> 32);
            }

            if (block_off != 0 || chunk != bsize) {
                if (!read_block(pblock, block_buf, bsize)) {
                    kernel::memory::free(block_buf);
                    return Result<usize>::err(Error::Io);
                }
            }

            memcpy(block_buf + block_off, src, chunk);

            if (!write_block(pblock, block_buf, bsize)) {
                kernel::memory::free(block_buf);
                return Result<usize>::err(Error::Io);
            }

            last_phys = pblock;
            src += chunk;
            cur_off += chunk;
            remaining -= chunk;
        }

        kernel::memory::free(block_buf);

        const u64 new_end = offset + size;
        if (new_end > inode_get_size(inode)) inode_set_size(inode, new_end);

        time::update_write(inode);

        if (!write_inode(inode_number, inode)) return Result<usize>::err(Error::Io);

        return Result<usize>::ok(static_cast<i64>(size));
    }

    namespace {
        struct Ext4NodeKind {
            InodeType inode_type;
            DirEntryType dirent_type;
        };

        Ext4NodeKind classify_create_mode(mode_t mode) {
            switch (mode & 0xF000u) {
                case static_cast<u32>(InodeType::Fifo):
                    return {InodeType::Fifo, DirEntryType::Fifo};
                default:
                    return {InodeType::RegularFile, DirEntryType::RegularFile};
            }
        }
    } // namespace

    Result<u32> FileSystem::create_file(u32 dir_inode_no, const char* name, mode_t mode) {
        if (!name || name[0] == '\0') return Result<u32>::err(Error::Inval);
        if (strlen(name) > EXT4_NAME_LEN) return Result<u32>::err(Error::NameTooLong);

        const u32 existing = dir_find_entry(dir_inode_no, name);
        if (existing != 0) return Result<u32>::err(Error::Exist);

        const u32 parent_group = (dir_inode_no - 1) / superblock_.s_inodes_per_group;
        const u32 new_inode = alloc_inode(parent_group);
        if (new_inode == 0) return Result<u32>::err(Error::NoSpc);

        const Ext4NodeKind kind = classify_create_mode(mode);

        const u16 perm_bits = static_cast<u16>(mode & 07777u);
        const u16 inode_mode = static_cast<u16>(kind.inode_type) | perm_bits;
        if (!init_inode(new_inode, inode_mode)) return Result<u32>::err(Error::Io);

        if (!dir_add_entry(dir_inode_no, name, new_inode, kind.dirent_type)) {
            free_inode(new_inode);
            return Result<u32>::err(Error::Io);
        }

        return Result<u32>::ok(new_inode);
    }

    Result<u32> FileSystem::create_dir(u32 dir_inode_no, const char* name, mode_t mode) {
        if (!name || name[0] == '\0') return Result<u32>::err(Error::Inval);
        if (strlen(name) > EXT4_NAME_LEN) return Result<u32>::err(Error::NameTooLong);

        const u32 existing = dir_find_entry(dir_inode_no, name);
        if (existing != 0) return Result<u32>::err(Error::Exist);

        const u32 parent_group = (dir_inode_no - 1) / superblock_.s_inodes_per_group;
        const u32 new_inode = alloc_inode(parent_group);
        if (new_inode == 0) return Result<u32>::err(Error::NoSpc);

        const u16 perm_bits = static_cast<u16>(mode & 07777u);
        const u16 inode_mode = static_cast<u16>(InodeType::Directory) | perm_bits;
        if (!init_inode(new_inode, inode_mode)) return Result<u32>::err(Error::Io);

        const u32 bsize = get_block_size();
        const u64 new_pblock = alloc_block(0);
        if (new_pblock == 0) return Result<u32>::err(Error::NoSpc);

        auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!block_buf) return Result<u32>::err(Error::NoMem);
        memset(block_buf, 0, bsize);

        // "." — zeigt auf neue Inode
        constexpr u16 dot_rec = (sizeof(DirEntry) + 1u + 3u) & ~3u;
        auto* dot = reinterpret_cast<DirEntry*>(block_buf);
        dot->inode = new_inode;
        dot->rec_len = dot_rec;
        dot->name_len = 1;
        dot->file_type = static_cast<u8>(DirEntryType::Directory);
        dot->name[0] = '.';

        // ".." — zeigt auf Eltern-Inode
        constexpr u16 dotdot_rec = (sizeof(DirEntry) + 2u + 3u) & ~3u;
        auto* dotdot = reinterpret_cast<DirEntry*>(block_buf + dot_rec);
        dotdot->inode = dir_inode_no;
        dotdot->rec_len = static_cast<u16>(bsize - dot_rec);
        dotdot->name_len = 2;
        dotdot->file_type = static_cast<u8>(DirEntryType::Directory);
        dotdot->name[0] = '.';
        dotdot->name[1] = '.';

        if (!write_block(new_pblock, block_buf, bsize)) {
            kernel::memory::free(block_buf);
            return Result<u32>::err(Error::Io);
        }
        kernel::memory::free(block_buf);

        Inode new_dir_inode{};
        if (!read_inode(new_inode, new_dir_inode)) return Result<u32>::err(Error::Io);

        if (!extent_tree_append(new_dir_inode, 0, new_pblock)) return Result<u32>::err(Error::NoSpc);

        const u64 blocks_512 = bsize / 512;
        new_dir_inode.i_blocks_lo = static_cast<u32>(blocks_512);
        new_dir_inode.i_blocks_high = 0;
        new_dir_inode.i_links_count = 2;
        inode_set_size(new_dir_inode, bsize);

        if (!write_inode(new_inode, new_dir_inode)) return Result<u32>::err(Error::Io);

        if (!dir_add_entry(dir_inode_no, name, new_inode, DirEntryType::Directory)) return Result<u32>::err(Error::Io);

        Inode parent_inode{};
        if (read_inode(dir_inode_no, parent_inode)) {
            parent_inode.i_links_count++;
            write_inode(dir_inode_no, parent_inode);
        }

        GroupDesc gd{};
        if (read_group_desc(parent_group, gd)) {
            gd.bg_used_dirs_count_lo++;
            write_group_desc(parent_group, gd);
        }

        return Result<u32>::ok(new_inode);
    }

    VoidResult FileSystem::unlink(u32 dir_inode_no, const char* name) {
        usize count = 0;
        auto dir_res = read_directory(dir_inode_no, count);
        if (dir_res.is_err()) return VoidResult::err(dir_res.error());

        FileEntry* entries = dir_res.unwrap();
        u32 target_inode = 0;

        for (usize i = 0; i < count; ++i) {
            if (strcmp(entries[i].name, name) == 0) {
                if (entries[i].is_dir()) {
                    kernel::memory::free(entries);
                    return VoidResult::err(Error::IsDir);
                }
                target_inode = entries[i].inode;
                break;
            }
        }
        kernel::memory::free(entries);

        if (target_inode == 0) return VoidResult::err(Error::NoEnt);

        if (!dir_remove_entry(dir_inode_no, name)) return VoidResult::err(Error::Io);

        Inode inode{};
        if (!read_inode(target_inode, inode)) return VoidResult::err(Error::Io);

        if (inode.i_links_count > 0) inode.i_links_count--;

        if (inode.i_links_count == 0) {
            free_blocks_for_inode(inode);
            inode.i_dtime = 0;
            write_inode(target_inode, inode);
            free_inode(target_inode);
        } else {
            write_inode(target_inode, inode);
        }

        return VoidResult::ok();
    }

    VoidResult FileSystem::rmdir(u32 dir_inode_no, const char* name) {
        usize count = 0;
        auto dir_res = read_directory(dir_inode_no, count);
        if (dir_res.is_err()) return VoidResult::err(dir_res.error());

        FileEntry* entries = dir_res.unwrap();
        u32 target_inode = 0;

        for (usize i = 0; i < count; ++i) {
            if (strcmp(entries[i].name, name) == 0) {
                if (!entries[i].is_dir()) {
                    kernel::memory::free(entries);
                    return VoidResult::err(Error::NotDir);
                }
                target_inode = entries[i].inode;
                break;
            }
        }
        kernel::memory::free(entries);

        if (target_inode == 0) return VoidResult::err(Error::NoEnt);

        if (!dir_is_empty(target_inode)) return VoidResult::err(Error::NotEmpty);

        if (!dir_remove_entry(dir_inode_no, name)) return VoidResult::err(Error::Io);

        Inode target{};
        if (read_inode(target_inode, target)) {
            free_blocks_for_inode(target);
            free_inode(target_inode);
        }

        Inode parent{};
        if (read_inode(dir_inode_no, parent)) {
            if (parent.i_links_count > 0) parent.i_links_count--;
            write_inode(dir_inode_no, parent);
        }

        const u32 group = (target_inode - 1) / superblock_.s_inodes_per_group;
        GroupDesc gd{};
        if (read_group_desc(group, gd)) {
            if (gd.bg_used_dirs_count_lo > 0) gd.bg_used_dirs_count_lo--;
            write_group_desc(group, gd);
        }

        return VoidResult::ok();
    }

    VoidResult FileSystem::rename(u32 old_dir_inode, const char* old_name, u32 new_dir_inode, const char* new_name) {
        usize src_count = 0;
        auto src_res = read_directory(old_dir_inode, src_count);
        if (src_res.is_err()) return VoidResult::err(src_res.error());

        FileEntry* src_entries = src_res.unwrap();
        u32 src_inode = 0;
        bool src_is_dir = false;
        auto src_type = DirEntryType::Unknown;

        for (usize i = 0; i < src_count; ++i) {
            if (strcmp(src_entries[i].name, old_name) == 0) {
                src_inode = src_entries[i].inode;
                src_is_dir = src_entries[i].is_dir();
                src_type = src_entries[i].type;
                break;
            }
        }
        kernel::memory::free(src_entries);

        if (src_inode == 0) return VoidResult::err(Error::NoEnt);

        usize dst_count = 0;
        auto dst_res = read_directory(new_dir_inode, dst_count);

        u32 dst_inode = 0;
        bool dst_is_dir = false;

        if (dst_res.is_ok()) {
            FileEntry* dst_entries = dst_res.unwrap();
            for (usize i = 0; i < dst_count; ++i) {
                if (strcmp(dst_entries[i].name, new_name) == 0) {
                    dst_inode = dst_entries[i].inode;
                    dst_is_dir = dst_entries[i].is_dir();
                    break;
                }
            }
            kernel::memory::free(dst_entries);
        }

        if (src_inode == dst_inode && old_dir_inode == new_dir_inode) return VoidResult::ok();

        if (dst_inode != 0) {
            if (src_is_dir && !dst_is_dir) return VoidResult::err(Error::NotDir);
            if (!src_is_dir && dst_is_dir) return VoidResult::err(Error::IsDir);
            if (dst_is_dir && !dir_is_empty(dst_inode)) return VoidResult::err(Error::NotEmpty);
        }

        if (dst_inode != 0) {
            if (!dir_remove_entry(new_dir_inode, new_name)) return VoidResult::err(Error::Io);

            if (dst_is_dir) {
                Inode dst_inode_data{};
                if (read_inode(dst_inode, dst_inode_data)) {
                    free_blocks_for_inode(dst_inode_data);
                    free_inode(dst_inode);
                }

                Inode new_dir{};
                if (read_inode(new_dir_inode, new_dir)) {
                    if (new_dir.i_links_count > 0) new_dir.i_links_count--;
                    write_inode(new_dir_inode, new_dir);
                }

                const u32 group = (dst_inode - 1) / superblock_.s_inodes_per_group;
                GroupDesc gd{};
                if (read_group_desc(group, gd)) {
                    if (gd.bg_used_dirs_count_lo > 0) gd.bg_used_dirs_count_lo--;
                    write_group_desc(group, gd);
                }
            } else {
                Inode dst_inode_data{};
                if (read_inode(dst_inode, dst_inode_data)) {
                    if (dst_inode_data.i_links_count > 0) dst_inode_data.i_links_count--;
                    if (dst_inode_data.i_links_count == 0) {
                        free_blocks_for_inode(dst_inode_data);
                        write_inode(dst_inode, dst_inode_data);
                        free_inode(dst_inode);
                    } else {
                        write_inode(dst_inode, dst_inode_data);
                    }
                }
            }
        }

        if (!dir_add_entry(new_dir_inode, new_name, src_inode, src_type)) return VoidResult::err(Error::Io);

        if (!dir_remove_entry(old_dir_inode, old_name)) {
            dir_remove_entry(new_dir_inode, new_name);
            return VoidResult::err(Error::Io);
        }

        if (src_is_dir && old_dir_inode != new_dir_inode) {
            const u32 bsize = get_block_size();
            auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
            if (block_buf) {
                Inode src_inode_data{};
                if (read_inode(src_inode, src_inode_data)) {
                    u64 pblock = 0;
                    if (map_logical_to_physical(src_inode_data, 0, pblock) && read_block(pblock, block_buf, bsize)) {
                        auto* dot = reinterpret_cast<DirEntry*>(block_buf);
                        auto* dotdot = reinterpret_cast<DirEntry*>(block_buf + dot->rec_len);
                        dotdot->inode = new_dir_inode;
                        write_block(pblock, block_buf, bsize);
                    }
                }
                kernel::memory::free(block_buf);
            }

            Inode old_parent{};
            if (read_inode(old_dir_inode, old_parent)) {
                if (old_parent.i_links_count > 0) old_parent.i_links_count--;
                write_inode(old_dir_inode, old_parent);
            }

            Inode new_parent{};
            if (read_inode(new_dir_inode, new_parent)) {
                new_parent.i_links_count++;
                write_inode(new_dir_inode, new_parent);
            }
        }

        Inode src_inode_data{};
        if (read_inode(src_inode, src_inode_data)) {
            time::update_write(src_inode_data);
            write_inode(src_inode, src_inode_data);
        }

        return VoidResult::ok();
    }

    VoidResult FileSystem::stat(u32 inode_no, struct stat* out, u32 dev_id) const {
        if (!out || inode_no == 0) return VoidResult::err(Error::Inval);

        Inode inode{};
        if (!read_inode(inode_no, inode)) return VoidResult::err(Error::Io);

        memset(out, 0, sizeof(struct stat));

        out->st_ino = inode_no;
        out->st_dev = dev_id;
        out->st_mode = inode.i_mode;
        out->st_nlink = inode.i_links_count;
        out->st_uid = static_cast<u32>(inode.i_uid) | (static_cast<u32>(inode.i_uid_high) << 16);
        out->st_gid = static_cast<u32>(inode.i_gid) | (static_cast<u32>(inode.i_gid_high) << 16);

        out->st_size = inode_get_size(inode);
        out->st_blksize = get_block_size();

        out->st_blocks = static_cast<u64>(inode.i_blocks_lo) | (static_cast<u64>(inode.i_blocks_high) << 32);

        const auto node_type = inode_get_type(inode);
        if (node_type == InodeType::CharDevice || node_type == InodeType::BlockDevice) {
            out->st_rdev = inode.i_block[0];
        } else {
            out->st_rdev = 0;
        }

        out->st_atim.tv_sec = inode.i_atime;
        out->st_mtim.tv_sec = inode.i_mtime;
        out->st_ctim.tv_sec = inode.i_ctime;
        out->v_crtime = inode.i_crtime;

        if (inode_get_size(inode) > 128) {
            out->st_atim.tv_nsec = (inode.i_atime_extra & 0x3FFFFFFFu) >> 2;
            out->st_mtim.tv_nsec = (inode.i_mtime_extra & 0x3FFFFFFFu) >> 2;
            out->st_ctim.tv_nsec = (inode.i_ctime_extra & 0x3FFFFFFFu) >> 2;
        } else {
            out->st_atim.tv_nsec = 0;
            out->st_mtim.tv_nsec = 0;
            out->st_ctim.tv_nsec = 0;
        }

        out->v_flags = VSTAT_FLAG_READABLE;
        if (inode.i_mode & 0x0080u) {
            // S_IWUSR
            out->v_flags |= VSTAT_FLAG_WRITABLE;
        }

        switch (node_type) {
            case InodeType::RegularFile:
                if (inode.i_mode & 0x0040u) out->v_flags |= VSTAT_FLAG_EXEC; // S_IXUSR
                out->v_node_type = VSTAT_TYPE_FILE;
                break;
            case InodeType::Directory:
                out->v_node_type = VSTAT_TYPE_DIR;
                break;
            case InodeType::SymbolicLink:
                out->v_node_type = VSTAT_TYPE_SYMLINK;
                break;
            case InodeType::CharDevice:
                out->v_node_type = VSTAT_TYPE_CHARDEV;
                break;
            case InodeType::BlockDevice:
                out->v_node_type = VSTAT_TYPE_BLOCKDEV;
                break;
            default:
                out->v_node_type = VSTAT_TYPE_UNKNOWN;
                break;
        }

        return VoidResult::ok();
    }

    VoidResult FileSystem::truncate(u32 inode_no, u64 new_size) {
        Inode inode{};
        if (!read_inode(inode_no, inode)) return VoidResult::err(Error::Io);
        if (inode_get_type(inode) != InodeType::RegularFile) return VoidResult::err(Error::IsDir);

        const u64 old_size = inode_get_size(inode);
        const u32 bsize = get_block_size();

        if (new_size == old_size) return VoidResult::ok();

        if (new_size < old_size) {
            const u32 new_last_lblock = (new_size == 0) ? 0 : static_cast<u32>((new_size - 1) / bsize);

            Vector<ExtentMap> extents;
            if (!parse_extents_raw(inode, extents)) return VoidResult::err(Error::Io);

            for (const ExtentMap& em : extents) {
                for (u32 i = 0; i < em.length; ++i) {
                    if (new_size == 0 || (em.logical_start + i) > new_last_lblock) free_block(em.phys_start + i);
                }
            }

            auto* eh = reinterpret_cast<ExtentHeader*>(&inode.i_block[0]);
            eh->eh_magic = EXT4_EXTENT_MAGIC;
            eh->eh_entries = 0;
            eh->eh_max = EXT4_MAX_INLINE_EXTENTS;
            eh->eh_depth = 0;
            eh->eh_generation = 0;

            if (new_size > 0) {
                for (const ExtentMap& em : extents) {
                    u32 surviving = 0;
                    for (u32 i = 0; i < em.length; ++i)
                        if (em.logical_start + i <= new_last_lblock) ++surviving;
                    for (u32 i = 0; i < surviving; ++i)
                        extent_tree_append(inode, em.logical_start + i, em.phys_start + i);
                }
            }

            const u32 surviving_blocks = (new_size == 0) ? 0 : (new_last_lblock + 1);
            const u64 new_blocks_512 = static_cast<u64>(surviving_blocks) * (bsize / 512);
            inode.i_blocks_lo = static_cast<u32>(new_blocks_512 & 0xFFFFFFFFu);
            inode.i_blocks_high = static_cast<u16>(new_blocks_512 >> 32);
        }

        inode_set_size(inode, new_size);
        time::update_write(inode);

        if (!write_inode(inode_no, inode)) return VoidResult::err(Error::Io);

        return VoidResult::ok();
    }

    VoidResult FileSystem::chown(u32 inode_no, u32 uid, u32 gid) const {
        if (inode_no == 0) return VoidResult::err(Error::Inval);

        Inode inode{};
        if (!read_inode(inode_no, inode)) return VoidResult::err(Error::Io);

        inode.i_uid = static_cast<u16>(uid & 0xFFFFu);
        inode.i_uid_high = static_cast<u16>(uid >> 16);
        inode.i_gid = static_cast<u16>(gid & 0xFFFFu);
        inode.i_gid_high = static_cast<u16>(gid >> 16);

        time::update_change(inode);

        if (!write_inode(inode_no, inode)) return VoidResult::err(Error::Io);

        return VoidResult::ok();
    }

    VoidResult FileSystem::chmod(u32 inode_no, u16 new_mode) const {
        if (inode_no == 0) return VoidResult::err(Error::Inval);

        Inode inode{};
        if (!read_inode(inode_no, inode)) return VoidResult::err(Error::Io);

        constexpr u16 type_mask = 0xF000u;
        constexpr u16 perm_mask = 0x0FFFu;
        inode.i_mode = (inode.i_mode & type_mask) | (new_mode & perm_mask);

        time::update_change(inode);

        if (!write_inode(inode_no, inode)) return VoidResult::err(Error::Io);

        return VoidResult::ok();
    }
} // namespace ext4
