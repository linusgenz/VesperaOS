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
#include <memory.h>
#include <string.h>
#include <vector.h>

namespace EXT4 {
    FileSystem::FileSystem(BlockDevice *device) {
        this->device = device;
        this->valid = false;
        sectorSize = device->get_sector_size();
        valid = read_superblock();
    }

    bool FileSystem::read_superblock() {
        constexpr uint32_t SUPERBLOCK_OFFSET = 1024;
        uint32_t startSector = SUPERBLOCK_OFFSET / sectorSize;
        uint32_t sectorCount = (sizeof(Ext4Superblock) + sectorSize - 1) / sectorSize;

        uint8_t buffer[sectorCount * sectorSize];

        Log::debug("reading superblock... %u", sectorCount);
        if (!device->read(startSector, sectorCount, buffer)) {
            return false;
        }

        memcpy(&superblock, buffer, sizeof(Ext4Superblock));

        // Magic number
        return superblock.s_magic == EXT4_MAGIC;
    }

    bool FileSystem::read_block(uint64_t block, void *outBuf) {
        uint64_t bsize = get_block_size();
        uint64_t startByte = block * bsize;
        uint64_t startSector = startByte / sectorSize;
        uint32_t count = (bsize + sectorSize - 1) / sectorSize;
        return device->read(startSector, count, outBuf);
    }


    bool FileSystem::read_group_desc(uint32_t group, GroupDesc &gd) {
        uint32_t bsize = get_block_size();
        uint64_t gd_table_block = (superblock.s_first_data_block + 1);

        uint32_t desc_size = superblock.s_desc_size ? superblock.s_desc_size : 32;
        if (desc_size < 32) desc_size = 32;

        uint64_t gd_offset_bytes =
                (uint64_t) gd_table_block * bsize + (uint64_t) group * desc_size;
        uint64_t startSector = gd_offset_bytes / sectorSize;
        auto bytes_including_skew =
                (uint32_t) ((gd_offset_bytes % sectorSize) + sizeof(GroupDesc));
        uint32_t cnt = (bytes_including_skew + sectorSize - 1) / sectorSize;

        Log::debug("[ext4] read_group_desc: group=%u", group);
        Log::debug("[ext4]   -> bsize=%u, gd_table_block=%llu, desc_size=%u", bsize, gd_table_block, desc_size);
        Log::debug("[ext4]   -> gd_offset_bytes=%llu, startSector=%llu, bytes_including_skew=%u, cnt=%u",
                   (unsigned long long) gd_offset_bytes,
                   (unsigned long long) startSector,
                   bytes_including_skew,
                   cnt);

        uint8_t buf[cnt * sectorSize];

        if (!device->read(startSector, cnt, buf)) {
            Log::debug("[ext4] read_group_desc: device->read failed at startSector=%llu, cnt=%u",
                       (unsigned long long) startSector, cnt);
            return false;
        }

        Log::debug("[ext4] read_group_desc: device->read succeeded");

        memcpy(&gd, buf + (gd_offset_bytes % sectorSize), sizeof(GroupDesc));

        Log::debug("[ext4] read_group_desc: bg_inode_table_lo=%u, bg_block_bitmap_lo=%u, bg_inode_bitmap_lo=%u",
                   gd.bg_inode_table_lo, gd.bg_block_bitmap_lo, gd.bg_inode_bitmap_lo);

        return true;
    }


    bool FileSystem::read_inode(uint32_t inode_no, Inode &outInode) {
        if (inode_no == 0) {
            Log::debug("[ext4] read_inode: invalid inode_no=0");
            return false;
        }

        uint32_t inodes_per_group = superblock.s_inodes_per_group;
        uint32_t group = (inode_no - 1) / inodes_per_group;
        uint32_t index = (inode_no - 1) % inodes_per_group;

        Log::debug("[ext4] read_inode: inode_no=%u, inodes_per_group=%u, group=%u, index=%u",
                   inode_no, inodes_per_group, group, index);

        GroupDesc gd;
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

        uint32_t inode_size = (superblock.s_inode_size == 0) ? 128 : superblock.s_inode_size;
        uint64_t inode_table_offset = (uint64_t) inode_table_block * get_block_size();
        uint64_t inode_offset = inode_table_offset + (uint64_t) index * inode_size;

        uint64_t startSector = inode_offset / sectorSize;
        uint32_t count = (inode_size + sectorSize - 1) / sectorSize;

        Log::debug("[ext4] read_inode: inode_table_block=%llu, inode_size=%u, inode_offset=%llu",
                   (unsigned long long) inode_table_block, inode_size, (unsigned long long) inode_offset);
        Log::debug("[ext4] read_inode: startSector=%llu, count=%u, sectorSize=%u",
                   (unsigned long long) startSector, count, sectorSize);

        uint8_t buf[count * sectorSize];
        memset(buf, 0, count * sectorSize);

        if (!device->read(startSector, count, buf)) {
            Log::debug("[ext4] read_inode: device->read failed at startSector=%llu, count=%u",
                       (unsigned long long) startSector, count);
            return false;
        }

        Log::debug("[ext4] read_inode: device->read succeeded");

        memcpy(&outInode, buf + (inode_offset % sectorSize), sizeof(Inode));
        const Inode *test = reinterpret_cast<const Inode *>(buf);
        Log::debug("[ext4] read_inode: i_mode=%u, i_size_lo=%u, i_block[0]=%u",
                   outInode.i_mode, outInode.i_size_lo, outInode.i_block[0]);

        Log::debug("[ext4]  i_mode=%u, i_size_lo=%u, i_block[0]=%u", test->i_mode, test->i_size_lo, test->i_block[0]);

        return true;
    }


    bool FileSystem::parse_extents_from_inode(Inode &inode, Vector<Ext4ExtentMap> &outExtents) {
        ExtentHeader eh;
        memcpy(&eh, &inode.i_block[0], sizeof(ExtentHeader));
        if (eh.eh_magic != EXT4_EXTENT_MAGIC) return false;

        // if depth != 0 we would have to walk tree via index nodes (not implemented here)
        if (eh.eh_depth != 0) return false;

        uint8_t *base = (uint8_t *) &inode.i_block[0] + sizeof(ExtentHeader);
        for (int i = 0; i < eh.eh_entries; ++i) {
            Extent ex;
            memcpy(&ex, base + i * sizeof(Extent), sizeof(Extent));
            uint64_t start = ((uint64_t) ex.ee_start_hi << 32) | ex.ee_start_lo;
            uint32_t len = ex.ee_len & 0xFFFF;
            outExtents.push_back({len, start});
        }
        return true;
    }

    bool FileSystem::map_logical_to_physical(Inode &inode, uint32_t lblock, uint64_t &out_pblock) {
        Vector<Ext4ExtentMap> exts;
        if (parse_extents_from_inode(inode, exts)) {
            ExtentHeader eh;
            memcpy(&eh, &inode.i_block[0], sizeof(ExtentHeader));
            if (eh.eh_depth != 0) return false; // not handling interior nodes
            uint8_t *ptr = (uint8_t *) &inode.i_block[0] + sizeof(ExtentHeader);
            uint32_t cur_log = 0;
            for (int i = 0; i < eh.eh_entries; ++i) {
                Extent ex;
                memcpy(&ex, ptr + i * sizeof(Extent), sizeof(Extent));
                uint32_t ee_block = ex.ee_block;
                uint32_t len = ex.ee_len & 0xFFFF;
                uint64_t start = ((uint64_t) ex.ee_start_hi << 32) | ex.ee_start_lo;
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

    size_t FileSystem::read_directory(uint32_t inodeNumber, FileEntry *outEntries) {
        size_t outCount = 0;

        Inode dirInode;
        Log::debug("inodeNumber: %d", inodeNumber);
        if (!read_inode(inodeNumber, dirInode)) {
            Log::debug("[ext4] read_inode failed for inode %u", inodeNumber);
            return 0;
        }
        Log::debug("i_mode=0x%x, type_bits=0x%x", dirInode.i_mode, dirInode.i_mode & 0xF000);
        bool is_dir = ((dirInode.i_mode & EXT4_S_IFMT) == EXT4_S_IFDIR);
        if (!is_dir) {
            Log::debug("[ext4] inode %u is not a directory according to i_mode=0x%x", inodeNumber, dirInode.i_mode);
            // Optional: prüfe i_block[0] oder andere Heuristiken
            // z.B. wenn i_block[0] != 0 && i_size_lo > 0, könnte es ein Verzeichnis sein
        }

        while (true);

        if ((dirInode.i_mode & 0xF000) != EXT4_S_IFDIR) {
            Log::debug("[ext4] inode %u is not a directory (i_mode=0x%x)", inodeNumber, dirInode.i_mode);
            return 0;
        }

        uint32_t blockCount = (inode_get_size(dirInode) + get_block_size() - 1) / get_block_size();

        for (uint32_t lblock = 0; lblock < blockCount && outCount < READ_DIR_MAX_ENTRIES; ++lblock) {
            uint64_t pblock;
            if (!map_logical_to_physical(dirInode, lblock, pblock)) {
                Log::debug("[ext4] logical block %u not mapped to physical block", lblock);
                continue;
            }

            uint8_t buf[get_block_size()];
            if (!read_block(pblock, buf)) {
                Log::debug("[ext4] failed to read physical block %llu", pblock);
                continue;
            }

            size_t offset = 0;
            while (offset + sizeof(DirEntry) <= get_block_size() && outCount < READ_DIR_MAX_ENTRIES) {
                auto *de = reinterpret_cast<DirEntry *>(buf + offset);
                if (de->inode == 0 || de->rec_len == 0) break;


                FileEntry &fe = outEntries[outCount++];
                fe.SetInode(de->inode);
                fe.SetType(de->file_type);
                fe.SetName(de->name, de->name_len);

                offset += de->rec_len;
            }
        }

        return outCount;
    }


    FileSystem::~FileSystem() {
    }
}
