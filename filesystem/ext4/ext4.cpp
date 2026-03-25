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

#include <klib/vector.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

namespace ext4 {

    FileSystem::FileSystem(BlockDevice* device)
        : device_(device)
        , sector_size_(device->get_sector_size())
        , valid_(false) {
        valid_ = read_superblock();
    }

    FileSystem::~FileSystem() = default;

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
            Log::debug("[ext4] bad magic: 0x%x (expected 0x%x)", superblock_.s_magic, EXT4_MAGIC);
            return false;
        }

        return true;
    }

    bool FileSystem::read_block(const u64 block, void* out_buf, const u32 buf_size) const {
        const u64 bsize = get_block_size();
        const u64 start_byte = block * bsize;
        const u64 start_sector = start_byte / sector_size_;
        const u32 count = (bsize + sector_size_ - 1) / sector_size_;

        return device_->read(start_sector, count, out_buf, buf_size);
    }

    bool FileSystem::read_group_desc(u32 group, GroupDesc& out_gd) const {
        const u32 bsize = get_block_size();

        // The group descriptor table starts at block 1 for block sizes > 1024,
        // or block 2 when block size == 1024 (block 0 is boot, block 1 is superblock).
        const u64 gd_table_block = (bsize == 1024) ? 2 : 1;

        const u64 gd_offset = gd_table_block * bsize + static_cast<u64>(group) * sizeof(GroupDesc);
        const u64 start_sector = gd_offset / sector_size_;
        const u32 count = (sizeof(GroupDesc) + sector_size_ - 1) / sector_size_;
        const u32 buf_size = count * sector_size_;

        Log::debug(
            "[ext4] read_group_desc: group=%u bsize=%u gd_table_block=%llu offset=%llu sector=%llu",
            group,
            bsize,
            gd_table_block,
            gd_offset,
            start_sector
        );

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return false;

        const bool ok = device_->read(start_sector, count, buf, buf_size);
        if (ok) {
            memcpy(&out_gd, buf + (gd_offset % sector_size_), sizeof(GroupDesc));
            Log::debug(
                "[ext4] read_group_desc: bg_inode_table_lo=%u bg_block_bitmap_lo=%u bg_inode_bitmap_lo=%u",
                out_gd.bg_inode_table_lo,
                out_gd.bg_block_bitmap_lo,
                out_gd.bg_inode_bitmap_lo
            );
        } else {
            Log::debug("[ext4] read_group_desc: device read failed");
        }

        kernel::memory::free(buf);
        return ok;
    }

    bool FileSystem::read_inode(u32 inode_no, Inode& out_inode) const {
        if (inode_no == 0) {
            Log::debug("[ext4] read_inode: inode 0 is invalid");
            return false;
        }

        const u32 inodes_per_group = superblock_.s_inodes_per_group;
        const u32 group = (inode_no - 1) / inodes_per_group;
        const u32 index = (inode_no - 1) % inodes_per_group;

        Log::debug("[ext4] read_inode: inode=%u group=%u index=%u", inode_no, group, index);

        GroupDesc gd{};
        if (!read_group_desc(group, gd)) {
            Log::debug("[ext4] read_inode: read_group_desc failed for group=%u", group);
            return false;
        }

        u64 inode_table_block = gd.bg_inode_table_lo;
        if (inode_table_block == 0) {
            inode_table_block = (get_block_size() == 1024) ? 5 : 1;
            Log::debug("[ext4] read_inode: using fallback inode_table_block=%llu", inode_table_block);
        }

        const u32 inode_size = (superblock_.s_inode_size == 0) ? DEFAULT_INODE_SIZE : superblock_.s_inode_size;
        const u64 table_offset = inode_table_block * get_block_size();
        const u64 inode_offset = table_offset + static_cast<u64>(index) * inode_size;

        const u64 start_sector = inode_offset / sector_size_;
        const u32 count = (inode_size + sector_size_ - 1) / sector_size_;
        const u32 buf_size = count * sector_size_;

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return false;

        const bool ok = device_->read(start_sector, count, buf, buf_size);
        if (ok) {
            memcpy(&out_inode, buf + (inode_offset % sector_size_), sizeof(Inode));
            Log::debug(
                "[ext4] read_inode: ok inode=%u i_mode=0x%x i_size_lo=%u",
                inode_no,
                out_inode.i_mode,
                out_inode.i_size_lo
            );
        } else {
            Log::debug("[ext4] read_inode: device read failed");
        }

        kernel::memory::free(buf);
        return ok;
    }

    bool FileSystem::parse_extents(const Inode& inode, Vector<ExtentMap>& out_extents) {
        ExtentHeader eh{};
        memcpy(&eh, &inode.i_block[0], sizeof(ExtentHeader));

        if (eh.eh_magic != EXT4_EXTENT_MAGIC) return false;
        if (eh.eh_depth != 0) return false;  // TODO: walk interior index nodes

        const auto* base = reinterpret_cast<const u8*>(&inode.i_block[0]) + sizeof(ExtentHeader);
        for (u16 i = 0; i < eh.eh_entries; ++i) {
            Extent ex{};
            memcpy(&ex, base + i * sizeof(Extent), sizeof(Extent));

            const u64 phys_start = (static_cast<u64>(ex.ee_start_hi) << 32) | ex.ee_start_lo;
            const u32 len = ex.ee_len & 0x7FFF;  // strip the unwritten flag

            out_extents.push_back({ex.ee_block, len, phys_start});
        }
        return true;
    }

    bool FileSystem::map_logical_to_physical(const Inode& inode, u32 lblock, u64& out_pblock) const {
        Vector<ExtentMap> extents;
        if (parse_extents(inode, extents)) {
            for (const ExtentMap& ext : extents) {
                if (lblock >= ext.logical_start && lblock < ext.logical_start + ext.length) {
                    out_pblock = ext.phys_start + (lblock - ext.logical_start);
                    return true;
                }
            }
            return false;
        }

        // Fallback: direct block pointers (i_block[0..11]).
        if (lblock < 12) {
            const u32 p = inode.i_block[lblock];
            if (p == 0) return false;
            out_pblock = p;
            return true;
        }

        return false;
    }

    FileEntry* FileSystem::read_directory(const u32 inode_number, usize& out_count) const {
        out_count = 0;
        Log::debug("[ext4] read_directory: inode=%u", inode_number);

        Inode dir_inode{};
        if (!read_inode(inode_number, dir_inode)) {
            Log::debug("[ext4] read_directory: read_inode failed for inode=%u", inode_number);
            return nullptr;
        }

        if (inode_get_type(dir_inode) != InodeType::Directory) {
            Log::debug(
                "[ext4] read_directory: inode=%u is not a directory (i_mode=0x%x)", inode_number, dir_inode.i_mode
            );
            return nullptr;
        }

        auto* entries = static_cast<FileEntry*>(kernel::memory::malloc(sizeof(FileEntry) * EXT4_MAX_DIR_ENTRIES));
        if (!entries) {
            Log::debug("[ext4] read_directory: malloc failed");
            return nullptr;
        }

        const u32 bsize = get_block_size();
        const u64 dir_size = inode_get_size(dir_inode);
        const u32 block_count = static_cast<u32>((dir_size + bsize - 1) / bsize);

        auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!block_buf) {
            kernel::memory::free(entries);
            return nullptr;
        }

        for (u32 lblock = 0; lblock < block_count && out_count < EXT4_MAX_DIR_ENTRIES; ++lblock) {
            u64 pblock = 0;
            if (!map_logical_to_physical(dir_inode, lblock, pblock)) {
                Log::debug("[ext4] read_directory: logical block %u not mapped", lblock);
                continue;
            }

            if (!read_block(pblock, block_buf, bsize)) {
                Log::debug("[ext4] read_directory: failed to read physical block %llu", pblock);
                continue;
            }

            usize offset = 0;
            while (offset + sizeof(DirEntry) <= bsize && out_count < EXT4_MAX_DIR_ENTRIES) {
                const auto* de = reinterpret_cast<const DirEntry*>(block_buf + offset);

                if (de->rec_len == 0) break;  // corrupted entry; stop parsing this block
                if (de->inode == 0) {         // deleted entry; skip
                    offset += de->rec_len;
                    continue;
                }

                FileEntry& fe = entries[out_count++];
                fe.set_inode(de->inode);
                fe.set_type(de->file_type);
                fe.set_name(de->name, de->name_len);

                offset += de->rec_len;
            }
        }

        kernel::memory::free(block_buf);
        Log::debug("[ext4] read_directory: found %zu entries", out_count);
        return entries;
    }

}  // namespace ext4