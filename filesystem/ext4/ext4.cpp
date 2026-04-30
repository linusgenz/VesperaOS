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
#include <vespera/log.h>
#include <vespera/mm/memory.h>

#include "ext4_time.h"
#include "klib/result.h"
#include "uapi/vespera/stat.h"

namespace ext4 {

    FileSystem::FileSystem(BlockDevice* device)
        : device_(device)
        , sector_size_(device->get_sector_size())
        , valid_(false) {
        valid_ = read_superblock().is_ok();
    }

    FileSystem::~FileSystem() = default;

    // =========================================================================
    // Superblock
    // =========================================================================

    Result<void> FileSystem::read_superblock() {
        const u32 start_sector = SUPERBLOCK_OFFSET / sector_size_;
        const u32 sector_count = (sizeof(Ext4Superblock) + sector_size_ - 1) / sector_size_;
        const u32 buf_size = sector_count * sector_size_;

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return Result<void>::err(Error::ENOMEM);

        const bool ok = device_->read(start_sector, sector_count, buf, buf_size);
        if (ok) memcpy(&superblock_, buf, sizeof(Ext4Superblock));
        kernel::memory::free(buf);

        if (!ok) return Result<void>::err(Error::EIO);

        if (superblock_.s_magic != EXT4_MAGIC) return Result<void>::err(Error::EIO);

        return Result<void>::ok();
    }

    Result<void> FileSystem::write_superblock() const {
        const u32 start_sector = SUPERBLOCK_OFFSET / sector_size_;
        const u32 sector_count = (sizeof(Ext4Superblock) + sector_size_ - 1) / sector_size_;
        const u32 buf_size = sector_count * sector_size_;

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return Result<void>::err(Error::ENOMEM);

        bool ok = device_->read(start_sector, sector_count, buf, buf_size);
        if (ok) {
            memcpy(buf + (SUPERBLOCK_OFFSET % sector_size_), &superblock_, sizeof(Ext4Superblock));
            ok = device_->write(start_sector, sector_count, buf, buf_size);
        }

        kernel::memory::free(buf);
        return ok ? Result<void>::ok() : Result<void>::err(Error::EIO);
    }

    // =========================================================================
    // Block I/O
    // =========================================================================

    Result<void> FileSystem::read_block(u64 block, void* buf, u32 buf_size) const {
        const u64 bsize = get_block_size();
        const u64 start_byte = block * bsize;
        const u64 start_sector = start_byte / sector_size_;
        const u32 count = (bsize + sector_size_ - 1) / sector_size_;

        return device_->read(start_sector, count, buf, buf_size) ? Result<void>::ok() : Result<void>::err(Error::EIO);
    }

    Result<void> FileSystem::write_block(u64 block, const void* buf, u32 buf_size) const {
        const u64 bsize = get_block_size();
        const u64 start_byte = block * bsize;
        const u64 start_sector = start_byte / sector_size_;
        const u32 count = (bsize + sector_size_ - 1) / sector_size_;

        return device_->write(start_sector, count, buf, buf_size) ? Result<void>::ok() : Result<void>::err(Error::EIO);
    }

    // =========================================================================
    // Group Descriptor
    // =========================================================================

    static u64 group_desc_offset(u32 group, u32 bsize) {
        const u64 gd_table_block = (bsize == 1024) ? 2 : 1;
        return gd_table_block * bsize + static_cast<u64>(group) * sizeof(GroupDesc);
    }

    Result<void> FileSystem::read_group_desc(u32 group, GroupDesc& out_gd) const {
        const u32 bsize = get_block_size();
        const u64 gd_off = group_desc_offset(group, bsize);
        const u64 start = gd_off / sector_size_;
        const u32 count = (sizeof(GroupDesc) + sector_size_ - 1) / sector_size_;
        const u32 buf_size = count * sector_size_;

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return Result<void>::err(Error::ENOMEM);

        const bool ok = device_->read(start, count, buf, buf_size);
        if (ok) memcpy(&out_gd, buf + (gd_off % sector_size_), sizeof(GroupDesc));
        kernel::memory::free(buf);

        return ok ? Result<void>::ok() : Result<void>::err(Error::EIO);
    }

    Result<void> FileSystem::write_group_desc(u32 group, const GroupDesc& gd) const {
        const u32 bsize = get_block_size();
        const u64 gd_off = group_desc_offset(group, bsize);
        const u64 start = gd_off / sector_size_;
        const u32 count = (sizeof(GroupDesc) + sector_size_ - 1) / sector_size_;
        const u32 buf_size = count * sector_size_;

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return Result<void>::err(Error::ENOMEM);

        bool ok = device_->read(start, count, buf, buf_size);
        if (ok) {
            memcpy(buf + (gd_off % sector_size_), &gd, sizeof(GroupDesc));
            ok = device_->write(start, count, buf, buf_size);
        }

        kernel::memory::free(buf);
        return ok ? Result<void>::ok() : Result<void>::err(Error::EIO);
    }

    // =========================================================================
    // Inode I/O
    // =========================================================================

    Result<u64> FileSystem::inode_disk_offset(u32 inode_no, u32& out_inode_size) const {
        if (inode_no == 0) return Result<u64>::err(Error::EINVAL);

        const u32 inodes_per_group = superblock_.s_inodes_per_group;
        const u32 group = (inode_no - 1) / inodes_per_group;
        const u32 index = (inode_no - 1) % inodes_per_group;

        GroupDesc gd{};
        if (auto r = read_group_desc(group, gd); r.is_err()) return Result<u64>::err(r.err_code());

        u64 inode_table_block = gd.bg_inode_table_lo;
        if (inode_table_block == 0) inode_table_block = (get_block_size() == 1024) ? 5 : 1;

        out_inode_size = (superblock_.s_inode_size == 0) ? DEFAULT_INODE_SIZE : superblock_.s_inode_size;
        return Result<u64>::ok(inode_table_block * get_block_size() + static_cast<u64>(index) * out_inode_size);
    }

    Result<void> FileSystem::read_inode(u32 inode_no, Inode& out_inode) const {
        if (inode_no == 0) return Result<void>::err(Error::EINVAL);

        u32 inode_size = 0;
        auto offset_res = inode_disk_offset(inode_no, inode_size);
        if (offset_res.is_err()) return Result<void>::err(offset_res.err_code());
        const u64 inode_offset = offset_res.value();

        const u64 start_sector = inode_offset / sector_size_;
        const u32 count = (inode_size + sector_size_ - 1) / sector_size_;
        const u32 buf_size = count * sector_size_;

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return Result<void>::err(Error::ENOMEM);

        const bool ok = device_->read(start_sector, count, buf, buf_size);
        if (ok) memcpy(&out_inode, buf + (inode_offset % sector_size_), sizeof(Inode));
        kernel::memory::free(buf);

        return ok ? Result<void>::ok() : Result<void>::err(Error::EIO);
    }

    Result<void> FileSystem::write_inode(u32 inode_no, const Inode& inode) const {
        u32 inode_size = 0;
        auto offset_res = inode_disk_offset(inode_no, inode_size);
        if (offset_res.is_err()) return Result<void>::err(offset_res.err_code());
        const u64 inode_offset = offset_res.value();

        const u64 start_sector = inode_offset / sector_size_;
        const u32 count = (inode_size + sector_size_ - 1) / sector_size_;
        const u32 buf_size = count * sector_size_;

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return Result<void>::err(Error::ENOMEM);

        bool ok = device_->read(start_sector, count, buf, buf_size);
        if (ok) {
            memcpy(buf + (inode_offset % sector_size_), &inode, sizeof(Inode));
            ok = device_->write(start_sector, count, buf, buf_size);
        }

        kernel::memory::free(buf);
        return ok ? Result<void>::ok() : Result<void>::err(Error::EIO);
    }

    Result<u32> FileSystem::alloc_inode(u32 preferred_group) {
        const u32 bsize = get_block_size();
        const u32 inodes_per_group = superblock_.s_inodes_per_group;
        const u32 total_groups = (superblock_.s_inodes_count + inodes_per_group - 1) / inodes_per_group;

        auto* bitmap = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!bitmap) return Result<u32>::err(Error::ENOMEM);

        for (u32 pass = 0; pass < total_groups; ++pass) {
            const u32 group = (preferred_group + pass) % total_groups;

            GroupDesc gd{};
            if (read_group_desc(group, gd).is_err()) continue;
            if (gd.bg_free_inodes_count_lo == 0) continue;
            if (read_block(gd.bg_inode_bitmap_lo, bitmap, bsize).is_err()) continue;

            const u32 inodes_in_group = (group == total_groups - 1)
                                            ? ((superblock_.s_inodes_count - 1) % inodes_per_group + 1)
                                            : inodes_per_group;

            for (u32 byte = 0; byte < (inodes_in_group + 7) / 8; ++byte) {
                if (bitmap[byte] == 0xFF) continue;

                for (u32 bit = 0; bit < 8; ++bit) {
                    if (bitmap[byte] & (1u << bit)) continue;

                    const u32 inode_no = group * inodes_per_group + byte * 8 + bit + 1;
                    if (inode_no < EXT4_FIRST_INODE) continue;

                    bitmap[byte] |= (1u << bit);
                    if (write_block(gd.bg_inode_bitmap_lo, bitmap, bsize).is_err()) {
                        kernel::memory::free(bitmap);
                        return Result<u32>::err(Error::EIO);
                    }

                    // Best-effort metadata updates — primary allocation already succeeded.
                    gd.bg_free_inodes_count_lo--;
                    write_group_desc(group, gd);
                    superblock_.s_free_inodes_count--;
                    write_superblock();

                    kernel::memory::free(bitmap);
                    return Result<u32>::ok(inode_no);
                }
            }
        }

        kernel::memory::free(bitmap);
        return Result<u32>::err(Error::ENOSPC);
    }

    Result<void> FileSystem::init_inode(u32 inode_no, u16 mode) {
        Inode inode{};
        memset(&inode, 0, sizeof(Inode));
        inode.i_mode = mode;
        inode.i_links_count = 1;
        time::set_creation(inode);

        auto* eh = reinterpret_cast<ExtentHeader*>(&inode.i_block[0]);
        eh->eh_magic = EXT4_EXTENT_MAGIC;
        eh->eh_entries = 0;
        eh->eh_max = EXT4_MAX_INLINE_EXTENTS;
        eh->eh_depth = 0;
        eh->eh_generation = 0;
        inode.i_flags |= 0x00080000u;  // EXT4_EXTENTS_FL

        return write_inode(inode_no, inode);
    }

    Result<void> FileSystem::free_inode(u32 inode_no) {
        if (inode_no == 0) return Result<void>::err(Error::EINVAL);

        const u32 bsize = get_block_size();
        const u32 inodes_per_group = superblock_.s_inodes_per_group;
        const u32 group = (inode_no - 1) / inodes_per_group;
        const u32 index = (inode_no - 1) % inodes_per_group;

        GroupDesc gd{};
        if (auto r = read_group_desc(group, gd); r.is_err()) return Result<void>::err(r.err_code());

        auto* bitmap = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!bitmap) return Result<void>::err(Error::ENOMEM);

        if (read_block(gd.bg_inode_bitmap_lo, bitmap, bsize).is_err()) {
            kernel::memory::free(bitmap);
            return Result<void>::err(Error::EIO);
        }

        const u32 byte = index / 8;
        const u32 bit = index % 8;

        if (!(bitmap[byte] & (1u << bit))) {
            kernel::memory::free(bitmap);
            return Result<void>::ok();  // Already free.
        }

        bitmap[byte] &= ~(1u << bit);

        if (write_block(gd.bg_inode_bitmap_lo, bitmap, bsize).is_err()) {
            kernel::memory::free(bitmap);
            return Result<void>::err(Error::EIO);
        }
        kernel::memory::free(bitmap);

        gd.bg_free_inodes_count_lo++;
        write_group_desc(group, gd);
        superblock_.s_free_inodes_count++;
        write_superblock();

        return Result<void>::ok();
    }

    // =========================================================================
    // Directory Entry Manipulation
    // =========================================================================

    Result<void> FileSystem::dir_add_entry(u32 dir_inode_no, const char* name, u32 child_inode, DirEntryType type) {
        const u32 bsize = get_block_size();
        const u8 name_len = static_cast<u8>(strlen(name));
        const u16 needed = static_cast<u16>((sizeof(DirEntry) + name_len + 3u) & ~3u);

        Inode dir_inode{};
        if (auto r = read_inode(dir_inode_no, dir_inode); r.is_err()) return Result<void>::err(r.err_code());

        auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!block_buf) return Result<void>::err(Error::ENOMEM);

        const u64 dir_size = inode_get_size(dir_inode);
        const u32 block_count = static_cast<u32>((dir_size + bsize - 1) / bsize);

        // Pass 1: find slack in existing blocks.
        for (u32 lblock = 0; lblock < block_count; ++lblock) {
            auto pblock_res = map_logical_to_physical(dir_inode, lblock);
            if (pblock_res.is_err()) continue;
            const u64 pblock = pblock_res.value();

            if (read_block(pblock, block_buf, bsize).is_err()) continue;

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
                        if (write_block(pblock, block_buf, bsize).is_err()) {
                            kernel::memory::free(block_buf);
                            return Result<void>::err(Error::EIO);
                        }
                        kernel::memory::free(block_buf);
                        return Result<void>::ok();
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

                    if (write_block(pblock, block_buf, bsize).is_err()) {
                        kernel::memory::free(block_buf);
                        return Result<void>::err(Error::EIO);
                    }
                    kernel::memory::free(block_buf);
                    return Result<void>::ok();
                }

                offset += de->rec_len;
            }
        }

        // Pass 2: allocate a new directory block.
        auto new_pblock_res = alloc_block(0);
        if (new_pblock_res.is_err()) {
            kernel::memory::free(block_buf);
            return Result<void>::err(new_pblock_res.err_code());
        }
        const u64 new_pblock = new_pblock_res.value();

        memset(block_buf, 0, bsize);
        auto* de = reinterpret_cast<DirEntry*>(block_buf);
        de->inode = child_inode;
        de->rec_len = static_cast<u16>(bsize);
        de->name_len = name_len;
        de->file_type = static_cast<u8>(type);
        memcpy(de->name, name, name_len);

        if (write_block(new_pblock, block_buf, bsize).is_err()) {
            kernel::memory::free(block_buf);
            return Result<void>::err(Error::EIO);
        }
        kernel::memory::free(block_buf);

        const u32 new_lblock = block_count;
        if (auto r = extent_tree_append(dir_inode, new_lblock, new_pblock); r.is_err())
            return Result<void>::err(r.err_code());

        const u64 old_blocks_512 =
            static_cast<u64>(dir_inode.i_blocks_lo) | (static_cast<u64>(dir_inode.i_blocks_high) << 32);
        const u64 new_blocks_512 = old_blocks_512 + bsize / 512;
        dir_inode.i_blocks_lo = static_cast<u32>(new_blocks_512 & 0xFFFFFFFFu);
        dir_inode.i_blocks_high = static_cast<u16>(new_blocks_512 >> 32);
        inode_set_size(dir_inode, dir_size + bsize);

        return write_inode(dir_inode_no, dir_inode);
    }

    Result<void> FileSystem::dir_remove_entry(u32 dir_inode_no, const char* name) const {
        const u32 bsize = get_block_size();
        const u8 name_len = static_cast<u8>(strlen(name));

        Inode dir_inode{};
        if (auto r = read_inode(dir_inode_no, dir_inode); r.is_err()) return Result<void>::err(r.err_code());

        auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!block_buf) return Result<void>::err(Error::ENOMEM);

        const u64 dir_size = inode_get_size(dir_inode);
        const u32 block_count = static_cast<u32>((dir_size + bsize - 1) / bsize);

        for (u32 lblock = 0; lblock < block_count; ++lblock) {
            auto pblock_res = map_logical_to_physical(dir_inode, lblock);
            if (pblock_res.is_err()) continue;
            const u64 pblock = pblock_res.value();

            if (read_block(pblock, block_buf, bsize).is_err()) continue;

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

                    if (write_block(pblock, block_buf, bsize).is_err()) {
                        kernel::memory::free(block_buf);
                        return Result<void>::err(Error::EIO);
                    }
                    kernel::memory::free(block_buf);
                    return Result<void>::ok();
                }

                prev = de;
                offset += de->rec_len;
            }
        }

        kernel::memory::free(block_buf);
        return Result<void>::err(Error::ENOENT);
    }

    bool FileSystem::dir_is_empty(u32 inode_no) const {
        usize count = 0;
        auto r = read_directory(inode_no, count);
        if (r.is_err()) return false;  // Cannot verify — treat as non-empty (safe).

        FileEntry* entries = r.value();
        bool empty = true;
        for (usize i = 0; i < count; ++i) {
            const char* n = entries[i].get_name();
            if (strcmp(n, ".") != 0 && strcmp(n, "..") != 0) {
                empty = false;
                break;
            }
        }
        kernel::memory::free(entries);
        return empty;
    }

    // =========================================================================
    // Extent Tree
    // =========================================================================

    Result<void> FileSystem::parse_extents(const Inode& inode, Vector<ExtentMap>& out_extents) {
        ExtentHeader eh{};
        memcpy(&eh, &inode.i_block[0], sizeof(ExtentHeader));

        if (eh.eh_magic != EXT4_EXTENT_MAGIC) return Result<void>::err(Error::EIO);

        if (eh.eh_depth != 0) return Result<void>::err(Error::EUNSUPPORTED);

        const auto* base = reinterpret_cast<const u8*>(&inode.i_block[0]) + sizeof(ExtentHeader);
        for (u16 i = 0; i < eh.eh_entries; ++i) {
            Extent ex{};
            memcpy(&ex, base + i * sizeof(Extent), sizeof(Extent));

            const u64 phys_start = (static_cast<u64>(ex.ee_start_hi) << 32) | ex.ee_start_lo;
            const u32 len = ex.ee_len & 0x7FFF;
            out_extents.push_back({ex.ee_block, len, phys_start});
        }
        return Result<void>::ok();
    }

    Result<u64> FileSystem::map_logical_to_physical(const Inode& inode, u32 lblock) const {
        Vector<ExtentMap> extents;
        if (parse_extents(inode, extents).is_ok()) {
            for (const ExtentMap& ext : extents) {
                if (lblock >= ext.logical_start && lblock < ext.logical_start + ext.length)
                    return Result<u64>::ok(ext.phys_start + (lblock - ext.logical_start));
            }
            return Result<u64>::err(Error::ENOENT);
        }

        // Fallback: direct block pointers (i_block[0..11]).
        if (lblock < 12) {
            const u32 p = inode.i_block[lblock];
            if (p == 0) return Result<u64>::err(Error::ENOENT);
            return Result<u64>::ok(p);
        }

        return Result<u64>::err(Error::ENOENT);
    }

    // =========================================================================
    // Block Allocation
    // =========================================================================

    Result<u64> FileSystem::alloc_block(u64 near_block) {
        const u32 bsize = get_block_size();
        const u32 blocks_per_group = superblock_.s_blocks_per_group;
        const u32 total_groups = (superblock_.s_blocks_count_lo + blocks_per_group - 1) / blocks_per_group;

        const u32 preferred_group =
            (near_block > 0) ? static_cast<u32>((near_block - superblock_.s_first_data_block) / blocks_per_group) : 0;

        auto* bitmap = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!bitmap) return Result<u64>::err(Error::ENOMEM);

        for (u32 pass = 0; pass < total_groups; ++pass) {
            const u32 group = (preferred_group + pass) % total_groups;

            GroupDesc gd{};
            if (read_group_desc(group, gd).is_err()) continue;
            if (gd.bg_free_blocks_count_lo == 0) continue;
            if (read_block(gd.bg_block_bitmap_lo, bitmap, bsize).is_err()) continue;

            for (u32 byte = 0; byte < blocks_per_group / 8; ++byte) {
                if (bitmap[byte] == 0xFF) continue;

                for (u32 bit = 0; bit < 8; ++bit) {
                    if (bitmap[byte] & (1u << bit)) continue;

                    bitmap[byte] |= (1u << bit);
                    if (write_block(gd.bg_block_bitmap_lo, bitmap, bsize).is_err()) {
                        kernel::memory::free(bitmap);
                        return Result<u64>::err(Error::EIO);
                    }

                    // Best-effort metadata updates.
                    gd.bg_free_blocks_count_lo--;
                    write_group_desc(group, gd);
                    superblock_.s_free_blocks_count_lo--;
                    write_superblock();

                    const u64 phys_block = static_cast<u64>(superblock_.s_first_data_block) +
                                           static_cast<u64>(group) * blocks_per_group + byte * 8 + bit;

                    kernel::memory::free(bitmap);
                    return Result<u64>::ok(phys_block);
                }
            }
        }

        kernel::memory::free(bitmap);
        return Result<u64>::err(Error::ENOSPC);
    }

    Result<void> FileSystem::free_block(u64 phys_block) {
        const u32 bsize = get_block_size();
        const u32 blocks_per_group = superblock_.s_blocks_per_group;

        const u64 relative = phys_block - superblock_.s_first_data_block;
        const u32 group = static_cast<u32>(relative / blocks_per_group);
        const u32 index = static_cast<u32>(relative % blocks_per_group);

        GroupDesc gd{};
        if (auto r = read_group_desc(group, gd); r.is_err()) return Result<void>::err(r.err_code());

        auto* bitmap = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!bitmap) return Result<void>::err(Error::ENOMEM);

        if (read_block(gd.bg_block_bitmap_lo, bitmap, bsize).is_err()) {
            kernel::memory::free(bitmap);
            return Result<void>::err(Error::EIO);
        }

        const u32 byte = index / 8;
        const u32 bit = index % 8;

        if (!(bitmap[byte] & (1u << bit))) {
            kernel::memory::free(bitmap);
            return Result<void>::ok();  // Already free.
        }

        bitmap[byte] &= ~(1u << bit);

        if (write_block(gd.bg_block_bitmap_lo, bitmap, bsize).is_err()) {
            kernel::memory::free(bitmap);
            return Result<void>::err(Error::EIO);
        }
        kernel::memory::free(bitmap);

        gd.bg_free_blocks_count_lo++;
        write_group_desc(group, gd);
        superblock_.s_free_blocks_count_lo++;
        write_superblock();

        return Result<void>::ok();
    }

    Result<void> FileSystem::free_blocks_for_inode(const Inode& inode) {
        Vector<ExtentMap> extents;
        if (parse_extents(inode, extents).is_ok()) {
            for (const ExtentMap& em : extents) {
                for (u32 i = 0; i < em.length; ++i)
                    free_block(em.phys_start + i);  // Best effort; ignore individual errors.
            }
            return Result<void>::ok();
        }

        // Fallback: direct block pointers.
        for (u32 i = 0; i < 12; ++i) {
            if (inode.i_block[i] != 0) free_block(inode.i_block[i]);
        }
        return Result<void>::ok();
    }

    Result<void> FileSystem::extent_tree_append(Inode& inode, u32 logical_block, u64 phys_block) {
        auto* eh = reinterpret_cast<ExtentHeader*>(&inode.i_block[0]);

        if (eh->eh_magic != EXT4_EXTENT_MAGIC) {
            eh->eh_magic = EXT4_EXTENT_MAGIC;
            eh->eh_entries = 0;
            eh->eh_max = EXT4_MAX_INLINE_EXTENTS;
            eh->eh_depth = 0;
            eh->eh_generation = 0;
        }

        if (eh->eh_depth != 0) return Result<void>::err(Error::EUNSUPPORTED);

        if (eh->eh_entries >= EXT4_MAX_INLINE_EXTENTS) return Result<void>::err(Error::ENOSPC);

        // Try to extend the last extent if blocks are contiguous.
        if (eh->eh_entries > 0) {
            auto* last = reinterpret_cast<Extent*>(
                reinterpret_cast<u8*>(eh) + sizeof(ExtentHeader) + (eh->eh_entries - 1) * sizeof(Extent)
            );

            const u64 last_phys_end =
                ((static_cast<u64>(last->ee_start_hi) << 32) | last->ee_start_lo) + (last->ee_len & 0x7FFFu);
            const u32 last_log_end = last->ee_block + (last->ee_len & 0x7FFFu);

            if (last_log_end == logical_block && last_phys_end == phys_block && (last->ee_len & 0x7FFFu) < 0x7FFF) {
                last->ee_len++;
                return Result<void>::ok();
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

        return Result<void>::ok();
    }

    // =========================================================================
    // Public Filesystem Operations
    // =========================================================================

    Result<FileEntry*> FileSystem::read_directory(u32 inode_number, usize& out_count) const {
        out_count = 0;

        Inode dir_inode{};
        if (auto r = read_inode(inode_number, dir_inode); r.is_err()) return Result<FileEntry*>::err(r.err_code());

        if (inode_get_type(dir_inode) != InodeType::Directory) return Result<FileEntry*>::err(Error::ENOTDIR);

        auto* entries = static_cast<FileEntry*>(kernel::memory::malloc(sizeof(FileEntry) * EXT4_MAX_DIR_ENTRIES));
        if (!entries) return Result<FileEntry*>::err(Error::ENOMEM);

        const u32 bsize = get_block_size();
        const u64 dir_size = inode_get_size(dir_inode);
        const u32 block_count = static_cast<u32>((dir_size + bsize - 1) / bsize);

        auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!block_buf) {
            kernel::memory::free(entries);
            return Result<FileEntry*>::err(Error::ENOMEM);
        }

        for (u32 lblock = 0; lblock < block_count && out_count < EXT4_MAX_DIR_ENTRIES; ++lblock) {
            auto pblock_res = map_logical_to_physical(dir_inode, lblock);
            if (pblock_res.is_err()) continue;
            const u64 pblock = pblock_res.value();

            if (read_block(pblock, block_buf, bsize).is_err()) continue;

            usize offset = 0;
            while (offset + sizeof(DirEntry) <= bsize && out_count < EXT4_MAX_DIR_ENTRIES) {
                const auto* de = reinterpret_cast<const DirEntry*>(block_buf + offset);
                if (de->rec_len == 0) break;
                if (de->inode == 0) {
                    offset += de->rec_len;
                    continue;
                }

                FileEntry& fe = entries[out_count++];
                fe.set_inode(de->inode);
                fe.set_type(de->file_type);
                fe.set_name(de->name, de->name_len);

                Inode file_inode{};
                if (read_inode(de->inode, file_inode).is_ok()) {
                    fe.set_size(inode_get_size(file_inode));
                    if (inode_get_type(file_inode) == InodeType::RegularFile)
                        fe.set_executable((file_inode.i_mode & 0x0040u) != 0);
                } else {
                    fe.set_size(0);
                }

                offset += de->rec_len;
            }
        }

        kernel::memory::free(block_buf);
        return Result<FileEntry*>::ok(entries);
    }

    Result<usize> FileSystem::read_file(u32 inode_number, u64 offset, usize size, void* buf, bool update_atime) const {
        Inode inode{};
        if (auto r = read_inode(inode_number, inode); r.is_err()) return Result<usize>::err(r.err_code());

        if (inode_get_type(inode) != InodeType::RegularFile) return Result<usize>::err(Error::EISDIR);

        const u64 file_size = inode_get_size(inode);
        if (offset >= file_size) return Result<usize>::ok(0);
        if (offset + size > file_size) size = static_cast<usize>(file_size - offset);

        const u32 bsize = get_block_size();
        auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!block_buf) return Result<usize>::err(Error::ENOMEM);

        auto* out = static_cast<u8*>(buf);
        usize remaining = size;
        u64 cur_off = offset;

        while (remaining > 0) {
            const u32 lblock = static_cast<u32>(cur_off / bsize);
            const u32 block_off = static_cast<u32>(cur_off % bsize);
            const usize chunk = (remaining < bsize - block_off) ? remaining : (bsize - block_off);

            auto pblock_res = map_logical_to_physical(inode, lblock);
            if (pblock_res.is_err()) {
                // Sparse hole — return zeroes.
                memset(out, 0, chunk);
            } else {
                if (read_block(pblock_res.value(), block_buf, bsize).is_err()) {
                    kernel::memory::free(block_buf);
                    return Result<usize>::err(Error::EIO);
                }
                memcpy(out, block_buf + block_off, chunk);
            }

            out += chunk;
            cur_off += chunk;
            remaining -= chunk;
        }

        if (update_atime) {
            time::update_access(inode);
            write_inode(inode_number, inode);
        }

        kernel::memory::free(block_buf);
        return Result<usize>::ok(size);
    }

    Result<usize> FileSystem::write_file(u32 inode_number, u64 offset, usize size, const void* buf) {
        Inode inode{};
        if (auto r = read_inode(inode_number, inode); r.is_err()) return Result<usize>::err(r.err_code());

        if (inode_get_type(inode) != InodeType::RegularFile) return Result<usize>::err(Error::EISDIR);

        const u32 bsize = get_block_size();
        auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!block_buf) return Result<usize>::err(Error::ENOMEM);

        const auto* src = static_cast<const u8*>(buf);
        usize remaining = size;
        u64 cur_off = offset;
        u64 last_phys = 0;

        while (remaining > 0) {
            const u32 lblock = static_cast<u32>(cur_off / bsize);
            const u32 block_off = static_cast<u32>(cur_off % bsize);
            const usize chunk = (remaining < bsize - block_off) ? remaining : (bsize - block_off);

            u64 pblock = 0;

            if (auto pblock_res = map_logical_to_physical(inode, lblock); pblock_res.is_err()) {
                // Allocate a new physical block.
                auto alloc_res = alloc_block(last_phys);
                if (alloc_res.is_err()) {
                    kernel::memory::free(block_buf);
                    const usize written = size - remaining;
                    if (written == 0) return Result<usize>::err(alloc_res.err_code());
                    write_inode(inode_number, inode);
                    return Result<usize>::ok(written);
                }
                pblock = alloc_res.value();

                memset(block_buf, 0, bsize);
                if (write_block(pblock, block_buf, bsize).is_err()) {
                    kernel::memory::free(block_buf);
                    return Result<usize>::err(Error::EIO);
                }

                if (auto r = extent_tree_append(inode, lblock, pblock); r.is_err()) {
                    kernel::memory::free(block_buf);
                    return Result<usize>::err(r.err_code());
                }

                const u64 blocks_512 =
                    static_cast<u64>(inode.i_blocks_lo) | (static_cast<u64>(inode.i_blocks_high) << 32);
                const u64 new_blocks = blocks_512 + bsize / 512;
                inode.i_blocks_lo = static_cast<u32>(new_blocks & 0xFFFFFFFFu);
                inode.i_blocks_high = static_cast<u16>(new_blocks >> 32);
            } else {
                pblock = pblock_res.value();
            }

            if (block_off != 0 || chunk != bsize) {
                if (read_block(pblock, block_buf, bsize).is_err()) {
                    kernel::memory::free(block_buf);
                    return Result<usize>::err(Error::EIO);
                }
            }

            memcpy(block_buf + block_off, src, chunk);

            if (write_block(pblock, block_buf, bsize).is_err()) {
                kernel::memory::free(block_buf);
                return Result<usize>::err(Error::EIO);
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
        if (auto r = write_inode(inode_number, inode); r.is_err()) return Result<usize>::err(r.err_code());

        return Result<usize>::ok(size);
    }

    Result<void> FileSystem::create_file(u32 dir_inode_no, const char* name) {
        const u32 parent_group = (dir_inode_no - 1) / superblock_.s_inodes_per_group;

        auto inode_res = alloc_inode(parent_group);
        if (inode_res.is_err()) return Result<void>::err(inode_res.err_code());
        const u32 new_inode = inode_res.value();

        constexpr u16 mode = static_cast<u16>(InodeType::RegularFile) | 0644u;
        if (auto r = init_inode(new_inode, mode); r.is_err()) return Result<void>::err(r.err_code());

        if (auto r = dir_add_entry(dir_inode_no, name, new_inode, DirEntryType::RegularFile); r.is_err()) {
            // TODO: free the inode on partial failure.
            return Result<void>::err(r.err_code());
        }

        return Result<void>::ok();
    }

    Result<void> FileSystem::create_dir(u32 dir_inode_no, const char* name) {
        const u32 parent_group = (dir_inode_no - 1) / superblock_.s_inodes_per_group;

        auto inode_res = alloc_inode(parent_group);
        if (inode_res.is_err()) return Result<void>::err(inode_res.err_code());
        const u32 new_inode = inode_res.value();

        constexpr u16 mode = static_cast<u16>(InodeType::Directory) | 0755u;
        if (auto r = init_inode(new_inode, mode); r.is_err()) return Result<void>::err(r.err_code());

        const u32 bsize = get_block_size();
        auto new_pblock_res = alloc_block(0);
        if (new_pblock_res.is_err()) return Result<void>::err(new_pblock_res.err_code());
        const u64 new_pblock = new_pblock_res.value();

        auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!block_buf) return Result<void>::err(Error::ENOMEM);
        memset(block_buf, 0, bsize);

        constexpr u16 dot_rec = (sizeof(DirEntry) + 1u + 3u) & ~3u;
        constexpr u16 dotdot_rec = (sizeof(DirEntry) + 2u + 3u) & ~3u;

        auto* dot = reinterpret_cast<DirEntry*>(block_buf);
        dot->inode = new_inode;
        dot->rec_len = dot_rec;
        dot->name_len = 1;
        dot->file_type = static_cast<u8>(DirEntryType::Directory);
        dot->name[0] = '.';

        auto* dotdot = reinterpret_cast<DirEntry*>(block_buf + dot_rec);
        dotdot->inode = dir_inode_no;
        dotdot->rec_len = static_cast<u16>(bsize - dot_rec);
        dotdot->name_len = 2;
        dotdot->file_type = static_cast<u8>(DirEntryType::Directory);
        dotdot->name[0] = '.';
        dotdot->name[1] = '.';

        if (write_block(new_pblock, block_buf, bsize).is_err()) {
            kernel::memory::free(block_buf);
            return Result<void>::err(Error::EIO);
        }
        kernel::memory::free(block_buf);

        Inode new_dir_inode{};
        if (auto r = read_inode(new_inode, new_dir_inode); r.is_err()) return Result<void>::err(r.err_code());

        if (auto r = extent_tree_append(new_dir_inode, 0, new_pblock); r.is_err())
            return Result<void>::err(r.err_code());

        const u64 blocks_512 = bsize / 512;
        new_dir_inode.i_blocks_lo = static_cast<u32>(blocks_512);
        new_dir_inode.i_blocks_high = 0;
        new_dir_inode.i_links_count = 2;
        inode_set_size(new_dir_inode, bsize);

        if (auto r = write_inode(new_inode, new_dir_inode); r.is_err()) return Result<void>::err(r.err_code());

        if (auto r = dir_add_entry(dir_inode_no, name, new_inode, DirEntryType::Directory); r.is_err())
            return Result<void>::err(r.err_code());

        // Increment parent link count for ".." back-reference (best effort).
        Inode parent_inode{};
        if (read_inode(dir_inode_no, parent_inode).is_ok()) {
            parent_inode.i_links_count++;
            write_inode(dir_inode_no, parent_inode);
        }

        // Increment group used-dir count (best effort).
        GroupDesc gd{};
        if (read_group_desc(parent_group, gd).is_ok()) {
            gd.bg_used_dirs_count_lo++;
            write_group_desc(parent_group, gd);
        }

        return Result<void>::ok();
    }

    Result<void> FileSystem::unlink(u32 dir_inode_no, const char* name) {
        usize count = 0;
        auto r = read_directory(dir_inode_no, count);
        if (r.is_err()) return Result<void>::err(r.err_code());
        FileEntry* entries = r.value();

        u32 target_inode = 0;
        for (usize i = 0; i < count; ++i) {
            if (strcmp(entries[i].get_name(), name) == 0) {
                if (entries[i].is_dir()) {
                    kernel::memory::free(entries);
                    return Result<void>::err(Error::EISDIR);
                }
                target_inode = entries[i].get_inode();
                break;
            }
        }
        kernel::memory::free(entries);

        if (target_inode == 0) return Result<void>::err(Error::ENOENT);

        if (auto dr = dir_remove_entry(dir_inode_no, name); dr.is_err()) return Result<void>::err(dr.err_code());

        Inode inode{};
        if (auto ir = read_inode(target_inode, inode); ir.is_err()) return Result<void>::err(ir.err_code());

        if (inode.i_links_count > 0) inode.i_links_count--;

        if (inode.i_links_count == 0) {
            free_blocks_for_inode(inode);
            inode.i_dtime = 0;
            write_inode(target_inode, inode);
            free_inode(target_inode);
        } else {
            write_inode(target_inode, inode);
        }

        return Result<void>::ok();
    }

    Result<void> FileSystem::rmdir(u32 dir_inode_no, const char* name) {
        usize count = 0;
        auto r = read_directory(dir_inode_no, count);
        if (r.is_err()) return Result<void>::err(r.err_code());
        FileEntry* entries = r.value();

        u32 target_inode = 0;
        for (usize i = 0; i < count; ++i) {
            if (strcmp(entries[i].get_name(), name) == 0) {
                if (!entries[i].is_dir()) {
                    kernel::memory::free(entries);
                    return Result<void>::err(Error::ENOTDIR);
                }
                target_inode = entries[i].get_inode();
                break;
            }
        }
        kernel::memory::free(entries);

        if (target_inode == 0) return Result<void>::err(Error::ENOENT);
        if (!dir_is_empty(target_inode)) return Result<void>::err(Error::ENOTEMPTY);

        if (auto dr = dir_remove_entry(dir_inode_no, name); dr.is_err()) return Result<void>::err(dr.err_code());

        Inode target{};
        if (read_inode(target_inode, target).is_ok()) {
            free_blocks_for_inode(target);
            free_inode(target_inode);
        }

        Inode parent{};
        if (read_inode(dir_inode_no, parent).is_ok()) {
            if (parent.i_links_count > 0) parent.i_links_count--;
            write_inode(dir_inode_no, parent);
        }

        const u32 group = (target_inode - 1) / superblock_.s_inodes_per_group;
        GroupDesc gd{};
        if (read_group_desc(group, gd).is_ok()) {
            if (gd.bg_used_dirs_count_lo > 0) gd.bg_used_dirs_count_lo--;
            write_group_desc(group, gd);
        }

        return Result<void>::ok();
    }

    Result<void> FileSystem::rename(u32 old_dir_inode, const char* old_name, u32 new_dir_inode, const char* new_name) {
        usize src_count = 0;
        auto src_r = read_directory(old_dir_inode, src_count);
        if (src_r.is_err()) return Result<void>::err(src_r.err_code());
        FileEntry* src_entries = src_r.value();

        u32 src_inode = 0;
        bool src_is_dir = false;
        DirEntryType src_type = DirEntryType::Unknown;

        for (usize i = 0; i < src_count; ++i) {
            if (strcmp(src_entries[i].get_name(), old_name) == 0) {
                src_inode = src_entries[i].get_inode();
                src_is_dir = src_entries[i].is_dir();
                src_type = src_entries[i].get_type();
                break;
            }
        }
        kernel::memory::free(src_entries);

        if (src_inode == 0) return Result<void>::err(Error::ENOENT);

        usize dst_count = 0;
        u32 dst_inode = 0;
        bool dst_is_dir = false;

        auto dst_r = read_directory(new_dir_inode, dst_count);
        if (dst_r.is_ok()) {
            FileEntry* dst_entries = dst_r.value();
            for (usize i = 0; i < dst_count; ++i) {
                if (strcmp(dst_entries[i].get_name(), new_name) == 0) {
                    dst_inode = dst_entries[i].get_inode();
                    dst_is_dir = dst_entries[i].is_dir();
                    break;
                }
            }
            kernel::memory::free(dst_entries);
        }

        if (src_inode == dst_inode && old_dir_inode == new_dir_inode) return Result<void>::ok();

        if (dst_inode != 0) {
            if (src_is_dir && !dst_is_dir) return Result<void>::err(Error::ENOTDIR);
            if (!src_is_dir && dst_is_dir) return Result<void>::err(Error::EISDIR);
            if (dst_is_dir && !dir_is_empty(dst_inode)) return Result<void>::err(Error::ENOTEMPTY);
        }

        if (dst_inode != 0) {
            if (auto dr = dir_remove_entry(new_dir_inode, new_name); dr.is_err())
                return Result<void>::err(dr.err_code());

            if (dst_is_dir) {
                Inode dst_inode_data{};
                if (read_inode(dst_inode, dst_inode_data).is_ok()) {
                    free_blocks_for_inode(dst_inode_data);
                    free_inode(dst_inode);
                }

                Inode new_dir{};
                if (read_inode(new_dir_inode, new_dir).is_ok()) {
                    if (new_dir.i_links_count > 0) new_dir.i_links_count--;
                    write_inode(new_dir_inode, new_dir);
                }

                const u32 group = (dst_inode - 1) / superblock_.s_inodes_per_group;
                GroupDesc gd{};
                if (read_group_desc(group, gd).is_ok()) {
                    if (gd.bg_used_dirs_count_lo > 0) gd.bg_used_dirs_count_lo--;
                    write_group_desc(group, gd);
                }
            } else {
                Inode dst_inode_data{};
                if (read_inode(dst_inode, dst_inode_data).is_ok()) {
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

        if (auto r = dir_add_entry(new_dir_inode, new_name, src_inode, src_type); r.is_err())
            return Result<void>::err(r.err_code());

        if (auto r = dir_remove_entry(old_dir_inode, old_name); r.is_err()) {
            dir_remove_entry(new_dir_inode, new_name);  // best-effort rollback
            return Result<void>::err(r.err_code());
        }

        if (src_is_dir && old_dir_inode != new_dir_inode) {
            const u32 bsize = get_block_size();
            auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
            if (block_buf) {
                Inode src_inode_data{};
                if (read_inode(src_inode, src_inode_data).is_ok()) {
                    auto pb_res = map_logical_to_physical(src_inode_data, 0);
                    if (pb_res.is_ok() && read_block(pb_res.value(), block_buf, bsize).is_ok()) {
                        auto* dot = reinterpret_cast<DirEntry*>(block_buf);
                        auto* dotdot = reinterpret_cast<DirEntry*>(block_buf + dot->rec_len);
                        dotdot->inode = new_dir_inode;
                        write_block(pb_res.value(), block_buf, bsize);
                    }
                }
                kernel::memory::free(block_buf);
            }

            Inode old_parent{};
            if (read_inode(old_dir_inode, old_parent).is_ok()) {
                if (old_parent.i_links_count > 0) old_parent.i_links_count--;
                write_inode(old_dir_inode, old_parent);
            }

            Inode new_parent{};
            if (read_inode(new_dir_inode, new_parent).is_ok()) {
                new_parent.i_links_count++;
                write_inode(new_dir_inode, new_parent);
            }
        }

        Inode src_inode_data{};
        if (read_inode(src_inode, src_inode_data).is_ok()) {
            time::update_write(src_inode_data);
            write_inode(src_inode, src_inode_data);
        }

        return Result<void>::ok();
    }

    // TODO add detection for groups, others, euid/egid regarding VSTAT flags
    Result<void> FileSystem::stat(u32 inode_no, vespera_stat_t* out, u32 dev_id) const {
        if (!out || inode_no == 0) return Result<void>::err(Error::EINVAL);

        Inode inode{};
        if (auto r = read_inode(inode_no, inode); r.is_err()) return Result<void>::err(r.err_code());

        out->inode_id = inode_no;
        out->size = inode_get_size(inode);
        out->blocks = static_cast<u64>(inode.i_blocks_lo) | (static_cast<u64>(inode.i_blocks_high) << 32);
        out->block_size = get_block_size();
        out->dev_id = dev_id;
        out->atime = inode.i_atime;
        out->mtime = inode.i_mtime;
        out->ctime = inode.i_ctime;
        out->crtime = inode.i_crtime;
        out->mode = inode.i_mode;
        out->links_count = inode.i_links_count;
        out->uid = static_cast<u32>(inode.i_uid) | (static_cast<u32>(inode.i_uid_high) << 16);
        out->gid = static_cast<u32>(inode.i_gid) | (static_cast<u32>(inode.i_gid_high) << 16);
        out->flags = VSTAT_FLAG_READABLE;
        if (inode.i_mode & 0x0080u) out->flags |= VSTAT_FLAG_WRITABLE;

        switch (inode_get_type(inode)) {
            case InodeType::RegularFile:
                if (inode.i_mode & 0x0040u) out->flags |= VSTAT_FLAG_EXEC;
                out->node_type = VSTAT_TYPE_FILE;
                break;
            case InodeType::Directory:
                out->node_type = VSTAT_TYPE_DIR;
                break;
            case InodeType::SymbolicLink:
                out->node_type = VSTAT_TYPE_SYMLINK;
                break;
            case InodeType::CharDevice:
                out->node_type = VSTAT_TYPE_CHARDEV;
                break;
            case InodeType::BlockDevice:
                out->node_type = VSTAT_TYPE_BLOCKDEV;
                break;
            default:
                out->node_type = VSTAT_TYPE_UNKNOWN;
                break;
        }

        return Result<void>::ok();
    }

    Result<void> FileSystem::truncate(u32 inode_no, u64 new_size) {
        Inode inode{};
        if (auto r = read_inode(inode_no, inode); r.is_err()) return Result<void>::err(r.err_code());

        if (inode_get_type(inode) != InodeType::RegularFile) return Result<void>::err(Error::EISDIR);

        const u64 old_size = inode_get_size(inode);
        const u32 bsize = get_block_size();

        if (new_size == old_size) return Result<void>::ok();

        if (new_size < old_size) {
            const u32 new_last_lblock = (new_size == 0) ? 0 : static_cast<u32>((new_size - 1) / bsize);

            Vector<ExtentMap> extents;
            if (auto r = parse_extents(inode, extents); r.is_err()) return Result<void>::err(r.err_code());

            for (const ExtentMap& em : extents) {
                for (u32 i = 0; i < em.length; ++i) {
                    const u32 lblock = em.logical_start + i;
                    if (new_size == 0 || lblock > new_last_lblock) free_block(em.phys_start + i);
                }
            }

            auto* eh = reinterpret_cast<ExtentHeader*>(&inode.i_block[0]);
            eh->eh_magic = EXT4_EXTENT_MAGIC;
            eh->eh_entries = 0;
            eh->eh_max = EXT4_MAX_INLINE_EXTENTS;
            eh->eh_depth = 0;

            if (new_size > 0) {
                for (const ExtentMap& em : extents) {
                    u32 surviving = 0;
                    for (u32 i = 0; i < em.length; ++i) {
                        if (em.logical_start + i <= new_last_lblock) ++surviving;
                    }
                    if (surviving == 0) continue;
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
        return write_inode(inode_no, inode);
    }

    Result<void> FileSystem::chown(u32 inode_no, u32 uid, u32 gid) const {
        if (inode_no == 0) return Result<void>::err(Error::EINVAL);

        Inode inode{};
        if (auto r = read_inode(inode_no, inode); r.is_err()) return Result<void>::err(r.err_code());

        inode.i_uid = static_cast<u16>(uid & 0xFFFFu);
        inode.i_uid_high = static_cast<u16>(uid >> 16);
        inode.i_gid = static_cast<u16>(gid & 0xFFFFu);
        inode.i_gid_high = static_cast<u16>(gid >> 16);
        time::update_change(inode);

        return write_inode(inode_no, inode);
    }

    Result<void> FileSystem::chmod(u32 inode_no, u16 new_mode) const {
        if (inode_no == 0) return Result<void>::err(Error::EINVAL);

        Inode inode{};
        if (auto r = read_inode(inode_no, inode); r.is_err()) return ::Result<void>::err(r.err_code());

        constexpr u16 type_mask = 0xF000u;
        constexpr u16 perm_mask = 0x0FFFu;
        inode.i_mode = (inode.i_mode & type_mask) | (new_mode & perm_mask);
        time::update_change(inode);

        return write_inode(inode_no, inode);
    }
}  // namespace ext4