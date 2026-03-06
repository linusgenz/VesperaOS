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

#include <log.h>
#include <vector.h>
#include <kernel/memory.h>

namespace ext4 {
    FileSystem::FileSystem(BlockDevice *device) {
        this->device_ = device;
        this->valid_ = false;
        sector_size_ = device->get_sector_size();
        valid_ = read_superblock();
    }

    bool FileSystem::read_superblock() {
        constexpr uint32_t superblock_offset = 1024;
        uint32_t start_sector = superblock_offset / sector_size_;
        uint32_t sector_count = (sizeof(Ext4Superblock) + sector_size_ - 1) / sector_size_;

        uint8_t buffer[sector_count * sector_size_];

        if (!device_->read(start_sector, sector_count, buffer, sizeof(buffer))) {
            return false;
        }

        memcpy(&superblock_, buffer, sizeof(Ext4Superblock));

        // Magic number
        return superblock_.s_magic == EXT4_MAGIC;
    }

    Ext4Superblock* FileSystem::get_superblock()
    {
        return &superblock_;
    }

    bool FileSystem::read_block(uint64_t block, void *out_buf) const
    {
        uint64_t bsize = get_block_size();
        uint64_t start_byte = block * bsize;
        uint64_t start_sector = start_byte / sector_size_;
        uint32_t count = (bsize + sector_size_ - 1) / sector_size_;
        return device_->read(start_sector, count, out_buf, sizeof(out_buf));
    }


    bool FileSystem::read_group_desc(uint32_t group, GroupDesc &gd) const
    {
        uint32_t bsize = get_block_size();
        uint64_t gd_table_block = 0;
        if (bsize == 1024) {
            gd_table_block = 2; // superblock bei Offset 1024 -> Block 1 belegt, Deskriptor ab Block 2
        } else {
            gd_table_block = 1;
        }

        const uint64_t gd_offset_bytes = gd_table_block * bsize + static_cast<uint64_t>(group) * sizeof(GroupDesc);
        const uint64_t start_sector = gd_offset_bytes / sector_size_;
        const uint32_t cnt = (sizeof(GroupDesc) + sector_size_ - 1) / sector_size_;

        Log::debug("[ext4] read_group_desc: group=%u bsize=%u gd_table_block=%llu",
                   group, bsize, static_cast<uint64_t>(gd_table_block));
        Log::debug("[ext4]   -> gd_offset_bytes=%llu startSector=%llu cnt=%u",
                   static_cast<uint64_t>(gd_offset_bytes),
                   static_cast<uint64_t>(start_sector),
                   cnt);

        auto *buf = static_cast<uint8_t*>(kernel::memory::malloc(cnt * sector_size_));
        if (!buf) {
            Log::debug("[ext4] read_group_desc: malloc failed (cnt=%u, sectorSize=%u)", cnt, sector_size_);
            return false;
        }

        if (!device_->read(start_sector, cnt, buf, sizeof(buf))) {
            Log::debug("[ext4] read_group_desc: device->read failed");
            kernel::memory::free(buf);
            return false;
        }

        memcpy(&gd, buf + (gd_offset_bytes % sector_size_), sizeof(GroupDesc));
        kernel::memory::free(buf);

        Log::debug("[ext4]   -> bg_inode_table_lo=%u bg_block_bitmap_lo=%u bg_inode_bitmap_lo=%u",
                   gd.bg_inode_table_lo, gd.bg_block_bitmap_lo, gd.bg_inode_bitmap_lo);

        return true;
    }


    bool FileSystem::read_inode(uint32_t inode_no, Inode &out_inode) const {
        if (inode_no == 0) {
            Log::debug("[ext4] read_inode: invalid inode_no=0");
            return false;
        }

        uint32_t inodes_per_group = superblock_.s_inodes_per_group;
        uint32_t group = (inode_no - 1) / inodes_per_group;
        uint32_t index = (inode_no - 1) % inodes_per_group;

        Log::debug("[ext4] read_inode: inode_no=%u inodes_per_group=%u group=%u index=%u",
                   inode_no, inodes_per_group, group, index);

        GroupDesc gd{};
        if (!read_group_desc(group, gd)) {
            Log::debug("[ext4] read_inode: read_group_desc failed for group=%u", group);
            return false;
        }

        uint64_t inode_table_block = gd.bg_inode_table_lo;

        // FIX für nackte Images ohne Partition
        if (inode_table_block == 0) {
            inode_table_block = (get_block_size() == 1024) ? 5 : 1;
            Log::debug("[ext4] read_inode: using fallback inode_table_block=%llu", inode_table_block);
        }

        const uint32_t inode_size = (superblock_.s_inode_size == 0) ? 128 : superblock_.s_inode_size;
        const uint64_t inode_table_offset = inode_table_block * get_block_size();
        const uint64_t inode_offset = inode_table_offset + static_cast<uint64_t>(index) * inode_size;

        uint64_t start_sector = inode_offset / sector_size_;
        uint32_t count = (inode_size + sector_size_ - 1) / sector_size_;

        auto *buf = static_cast<uint8_t*>(kernel::memory::malloc(count * sector_size_));
        if (!buf) return false;

        if (!device_->read(start_sector, count, buf, sizeof(buf))) {
            kernel::memory::free(buf);
            return false;
        }

        memcpy(&out_inode, buf + (inode_offset % sector_size_), sizeof(Inode));
        kernel::memory::free(buf);

        Log::debug("[ext4] read_inode: success inode_no=%u i_mode=%u i_size_lo=%u",
                   inode_no, out_inode.i_mode, out_inode.i_size_lo);

        return true;
    }



    bool FileSystem::parse_extents_from_inode(Inode &inode, Vector<Ext4ExtentMap> &out_extents) {
        ExtentHeader eh{};
        memcpy(&eh, &inode.i_block[0], sizeof(ExtentHeader));
        if (eh.eh_magic != EXT4_EXTENT_MAGIC) return false;

        // if depth != 0 we would have to walk tree via index nodes (not implemented here)
        if (eh.eh_depth != 0) return false;

        const uint8_t *base = reinterpret_cast<uint8_t*>(&inode.i_block[0]) + sizeof(ExtentHeader);
        for (int i = 0; i < eh.eh_entries; ++i) {
            Extent ex{};
            memcpy(&ex, base + i * sizeof(Extent), sizeof(Extent));
            const uint64_t start = (static_cast<uint64_t>(ex.ee_start_hi) << 32) | ex.ee_start_lo;
            const uint32_t len = ex.ee_len & 0xFFFF;
            out_extents.push_back({len, start});
        }
        return true;
    }

    bool FileSystem::map_logical_to_physical(Inode &inode, uint32_t lblock, uint64_t &out_pblock) {
        if (Vector<Ext4ExtentMap> exts; parse_extents_from_inode(inode, exts)) {
            ExtentHeader eh{};
            memcpy(&eh, &inode.i_block[0], sizeof(ExtentHeader));
            if (eh.eh_depth != 0) return false; // not handling interior nodes
            uint8_t *ptr = reinterpret_cast<uint8_t*>(&inode.i_block[0]) + sizeof(ExtentHeader);
            uint32_t cur_log = 0;
            for (int i = 0; i < eh.eh_entries; ++i) {
                Extent ex{};
                memcpy(&ex, ptr + i * sizeof(Extent), sizeof(Extent));
                uint32_t ee_block = ex.ee_block;
                uint32_t len = ex.ee_len & 0xFFFF;
                uint64_t start = (static_cast<uint64_t>(ex.ee_start_hi) << 32) | ex.ee_start_lo;
                // extent maps logical blocks starting at ee_block for len blocks to phys start
                if (lblock >= ee_block && lblock < ee_block + len) {
                    uint64_t offset = lblock - ee_block;
                    out_pblock = start + offset;
                    return true;
                }
            }
            return false;
        }

        // Fallback: direct blocks (i_block[0..11])
        if (lblock < 12) {
            uint32_t p = inode.i_block[lblock];
            if (p == 0) return false;
            out_pblock = p;
            return true;
        }
        // not supported: indirect blocks
        return false;
    }

    FileEntry* FileSystem::read_directory(uint32_t inode_number, size_t& out_count) {
        out_count = 0;

        Log::debug("[ext4] read_directory: inodeNumber=%u", inode_number);

        Inode dir_inode{};
        if (!read_inode(inode_number, dir_inode)) {
            Log::debug("[ext4] read_inode failed for inode %u", inode_number);
            return nullptr;
        }

        if ((dir_inode.i_mode & 0xF000) != EXT4_S_IFDIR) {
            Log::debug("[ext4] inode %u is not a directory (i_mode=0x%x)", inode_number, dir_inode.i_mode);
            return nullptr;
        }

        auto* entries = static_cast<FileEntry*>(kernel::memory::malloc(sizeof(FileEntry) * READ_DIR_MAX_ENTRIES));
        if (!entries) {
            Log::debug("[ext4] malloc failed for directory entries");
            return nullptr;
        }

        // Anzahl logischer Blöcke im Verzeichnis
        uint32_t block_count = (inode_get_size(dir_inode) + get_block_size() - 1) / get_block_size();

        for (uint32_t lblock = 0; lblock < block_count && out_count < READ_DIR_MAX_ENTRIES; ++lblock) {
            uint64_t pblock = 0;
            if (!map_logical_to_physical(dir_inode, lblock, pblock)) {
                Log::debug("[ext4] logical block %u not mapped to physical block", lblock);
                continue;
            }

            Vector<uint8_t> buf(get_block_size());
            if (!read_block(pblock, buf.data())) {
                Log::debug("[ext4] failed to read physical block %llu", pblock);
                continue;
            }

            size_t offset = 0;
            while (offset + sizeof(DirEntry) <= get_block_size() && out_count < READ_DIR_MAX_ENTRIES) {
                const auto* de = reinterpret_cast<DirEntry*>(buf.data() + offset);
                if (de->inode == 0 || de->rec_len == 0) break;

                FileEntry& fe = entries[out_count++];
                fe.set_inode(de->inode);
                fe.set_type(de->file_type);
                fe.set_name(de->name, de->name_len);

                offset += de->rec_len;
            }
        }

        Log::debug("[ext4] read_directory: read %zu entries", out_count);
        return entries;
    }



    FileSystem::~FileSystem() = default;
}
