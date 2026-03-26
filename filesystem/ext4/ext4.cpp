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

#include "ext4_time.h"

namespace ext4 {

    FileSystem::FileSystem(BlockDevice* device)
        : device_(device)
        , sector_size_(device->get_sector_size())
        , valid_(false) {
        valid_ = read_superblock();
    }

    FileSystem::~FileSystem() = default;

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
            //     Log::debug("[ext4] bad magic: 0x%x (expected 0x%x)", superblock_.s_magic, EXT4_MAGIC);
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

    // block io

    bool FileSystem::read_block(u64 block, void* buf, u32 buf_size) const {
        const u64 bsize = get_block_size();
        const u64 start_byte = block * bsize;
        const u64 start_sector = start_byte / sector_size_;
        const u32 count = (bsize + sector_size_ - 1) / sector_size_;
        return device_->read(start_sector, count, buf, buf_size);
    }

    bool FileSystem::write_block(u64 block, const void* buf, u32 buf_size) const {
        const u64 bsize = get_block_size();
        const u64 start_byte = block * bsize;
        const u64 start_sector = start_byte / sector_size_;
        const u32 count = (bsize + sector_size_ - 1) / sector_size_;
        return device_->write(start_sector, count, buf, buf_size);
    }

    // group descriptor

    static u64 group_desc_offset(u32 group, u32 bsize) {
        const u64 gd_table_block = (bsize == 1024) ? 2 : 1;
        return gd_table_block * bsize + static_cast<u64>(group) * sizeof(GroupDesc);
    }

    bool FileSystem::read_group_desc(u32 group, GroupDesc& out_gd) const {
        const u32 bsize = get_block_size();
        const u64 gd_offset = group_desc_offset(group, bsize);
        const u64 start_sect = gd_offset / sector_size_;
        const u32 count = (sizeof(GroupDesc) + sector_size_ - 1) / sector_size_;
        const u32 buf_size = count * sector_size_;

        //   Log::debug("[ext4] read_group_desc: group=%u offset=%llu sector=%llu", group, gd_offset, start_sect);

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return false;

        const bool ok = device_->read(start_sect, count, buf, buf_size);
        if (ok) {
            memcpy(&out_gd, buf + (gd_offset % sector_size_), sizeof(GroupDesc));
            /*Log::debug(
                "[ext4] read_group_desc: inode_table=%u block_bitmap=%u inode_bitmap=%u",
                out_gd.bg_inode_table_lo,
                out_gd.bg_block_bitmap_lo,
                out_gd.bg_inode_bitmap_lo
            );*/
        } else {
            Log::debug("[ext4] read_group_desc: device read failed");
        }

        kernel::memory::free(buf);
        return ok;
    }

    bool FileSystem::write_group_desc(u32 group, const GroupDesc& gd) const {
        const u32 bsize = get_block_size();
        const u64 gd_offset = group_desc_offset(group, bsize);
        const u64 start_sect = gd_offset / sector_size_;
        const u32 count = (sizeof(GroupDesc) + sector_size_ - 1) / sector_size_;
        const u32 buf_size = count * sector_size_;

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return false;

        // Read-modify-write so we don't corrupt adjacent descriptors.
        bool ok = device_->read(start_sect, count, buf, buf_size);
        if (ok) {
            memcpy(buf + (gd_offset % sector_size_), &gd, sizeof(GroupDesc));
            ok = device_->write(start_sect, count, buf, buf_size);
        }

        kernel::memory::free(buf);
        return ok;
    }

    // inode

    u64 FileSystem::inode_disk_offset(u32 inode_no, u32& out_inode_size) const {
        if (inode_no == 0) return 0;

        const u32 inodes_per_group = superblock_.s_inodes_per_group;
        const u32 group = (inode_no - 1) / inodes_per_group;
        const u32 index = (inode_no - 1) % inodes_per_group;

        GroupDesc gd{};
        if (!read_group_desc(group, gd)) return 0;

        u64 inode_table_block = gd.bg_inode_table_lo;
        if (inode_table_block == 0) {
            // Fallback for raw images without a partition table.
            inode_table_block = (get_block_size() == 1024) ? 5 : 1;
            Log::debug("[ext4] inode_disk_offset: fallback inode_table_block=%llu", inode_table_block);
        }

        out_inode_size = (superblock_.s_inode_size == 0) ? DEFAULT_INODE_SIZE : superblock_.s_inode_size;
        return inode_table_block * get_block_size() + static_cast<u64>(index) * out_inode_size;
    }

    bool FileSystem::read_inode(u32 inode_no, Inode& out_inode) const {
        if (inode_no == 0) {
            Log::debug("[ext4] read_inode: inode 0 is invalid");
            return false;
        }

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

    bool FileSystem::write_inode(u32 inode_no, const Inode& inode) const {
        u32 inode_size = 0;
        const u64 inode_offset = inode_disk_offset(inode_no, inode_size);
        if (inode_offset == 0) return false;

        const u64 start_sector = inode_offset / sector_size_;
        const u32 count = (inode_size + sector_size_ - 1) / sector_size_;
        const u32 buf_size = count * sector_size_;

        auto* buf = static_cast<u8*>(kernel::memory::malloc(buf_size));
        if (!buf) return false;

        // Read-modify-write to preserve any extra fields beyond sizeof(Inode).
        bool ok = device_->read(start_sector, count, buf, buf_size);
        if (ok) {
            memcpy(buf + (inode_offset % sector_size_), &inode, sizeof(Inode));
            ok = device_->write(start_sector, count, buf, buf_size);
        }

        kernel::memory::free(buf);
        return ok;
    }

    // extend tree

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

    // block allocation

    u64 FileSystem::alloc_block(u64 near_block) {
        const u32 bsize = get_block_size();
        const u32 blocks_per_group = superblock_.s_blocks_per_group;
        const u32 total_groups = (superblock_.s_blocks_count_lo + blocks_per_group - 1) / blocks_per_group;

        // Prefer the group that owns `near_block`.
        const u32 preferred_group =
            (near_block > 0) ? static_cast<u32>((near_block - superblock_.s_first_data_block) / blocks_per_group) : 0;

        // Bitmap buffer — one filesystem block large.
        auto* bitmap = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!bitmap) return 0;

        // Try the preferred group first, then fall through to all others.
        for (u32 pass = 0; pass < total_groups; ++pass) {
            const u32 group = (preferred_group + pass) % total_groups;

            GroupDesc gd{};
            if (!read_group_desc(group, gd)) continue;
            if (gd.bg_free_blocks_count_lo == 0) continue;

            if (!read_block(gd.bg_block_bitmap_lo, bitmap, bsize)) continue;

            // Scan the bitmap for the first free bit.
            for (u32 byte = 0; byte < blocks_per_group / 8; ++byte) {
                if (bitmap[byte] == 0xFF) continue;  // all 8 blocks used

                for (u32 bit = 0; bit < 8; ++bit) {
                    if (bitmap[byte] & (1u << bit)) continue;  // block is used

                    // Found a free block — mark it as used.
                    bitmap[byte] |= (1u << bit);
                    if (!write_block(gd.bg_block_bitmap_lo, bitmap, bsize)) {
                        kernel::memory::free(bitmap);
                        return 0;
                    }

                    // Update group descriptor free-block count.
                    gd.bg_free_blocks_count_lo--;
                    write_group_desc(group, gd);

                    // Update superblock free-block count.
                    superblock_.s_free_blocks_count_lo--;
                    write_superblock();

                    const u64 phys_block = static_cast<u64>(superblock_.s_first_data_block) +
                                           static_cast<u64>(group) * blocks_per_group + byte * 8 + bit;

                    kernel::memory::free(bitmap);
                    Log::debug(
                        "[ext4] alloc_block: allocated block=%llu (group=%u byte=%u bit=%u)",
                        phys_block,
                        group,
                        byte,
                        bit
                    );
                    return phys_block;
                }
            }
        }

        kernel::memory::free(bitmap);
        Log::debug("[ext4] alloc_block: no free blocks");
        return 0;
    }

    // append new lead extent

    bool FileSystem::extent_tree_append(Inode& inode, u32 logical_block, u64 phys_block) {
        auto* eh = reinterpret_cast<ExtentHeader*>(&inode.i_block[0]);

        // Initialise the extent tree header if this inode has never used extents.
        if (eh->eh_magic != EXT4_EXTENT_MAGIC) {
            eh->eh_magic = EXT4_EXTENT_MAGIC;
            eh->eh_entries = 0;
            eh->eh_max = EXT4_MAX_INLINE_EXTENTS;
            eh->eh_depth = 0;
            eh->eh_generation = 0;
        }

        if (eh->eh_depth != 0) {
            Log::debug("[ext4] extent_tree_append: depth>0 trees not supported");
            return false;
        }

        if (eh->eh_entries >= EXT4_MAX_INLINE_EXTENTS) {
            Log::debug("[ext4] extent_tree_append: inline extent array full (%u entries)", eh->eh_entries);
            return false;
        }

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
                return true;
            }
        }

        // Append a new extent entry.
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

    FileEntry* FileSystem::read_directory(const u32 inode_number, usize& out_count) const {
        out_count = 0;
        //   Log::debug("[ext4] read_directory: inode=%u", inode_number);

        Inode dir_inode{};
        if (!read_inode(inode_number, dir_inode)) {
            //       Log::debug("[ext4] read_directory: read_inode failed for inode=%u", inode_number);
            return nullptr;
        }

        if (inode_get_type(dir_inode) != InodeType::Directory) {
            //      Log::debug(
            //          "[ext4] read_directory: inode=%u is not a directory (i_mode=0x%x)", inode_number,
            //          dir_inode.i_mode
            //       );
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
                //        Log::debug("[ext4] read_directory: logical block %u not mapped", lblock);
                continue;
            }

            if (!read_block(pblock, block_buf, bsize)) {
                //      Log::debug("[ext4] read_directory: failed to read physical block %llu", pblock);
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
                if (Inode file_inode{}; read_inode(de->inode, file_inode)) {
                    fe.set_size(inode_get_size(file_inode));
                } else {
                    fe.set_size(0);
                }

                offset += de->rec_len;
            }
        }

        kernel::memory::free(block_buf);
        //      Log::debug("[ext4] read_directory: found %zu entries", out_count);
        return entries;
    }

    i64 FileSystem::read_file(u32 inode_number, u64 offset, usize size, void* buf) const {
        //  Log::debug("[ext4] read_file: inode=%u offset=%llu size=%zu", inode_number, offset, size);

        Inode inode{};
        if (!read_inode(inode_number, inode)) return -1;

        if (inode_get_type(inode) != InodeType::RegularFile) {
            //      Log::debug("[ext4] read_file: inode=%u is not a regular file", inode_number);
            return -1;
        }

        const u64 file_size = inode_get_size(inode);
        if (offset >= file_size) return 0;

        // Clamp to end of file.
        if (offset + size > file_size) size = static_cast<usize>(file_size - offset);

        const u32 bsize = get_block_size();
        auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!block_buf) return -1;

        auto* out = static_cast<u8*>(buf);
        usize remaining = size;
        u64 cur_off = offset;

        while (remaining > 0) {
            const u32 lblock = static_cast<u32>(cur_off / bsize);
            const u32 block_off = static_cast<u32>(cur_off % bsize);
            const usize chunk = (remaining < bsize - block_off) ? remaining : (bsize - block_off);

            u64 pblock = 0;
            if (!map_logical_to_physical(inode, lblock, pblock)) {
                // Sparse hole — return zeroes.
                memset(out, 0, chunk);
            } else {
                if (!read_block(pblock, block_buf, bsize)) {
                    kernel::memory::free(block_buf);
                    return -1;
                }
                memcpy(out, block_buf + block_off, chunk);
            }

            out += chunk;
            cur_off += chunk;
            remaining -= chunk;
        }

        kernel::memory::free(block_buf);
        Log::debug("[ext4] read_file: done, read %zu bytes", size);
        return static_cast<i64>(size);
    }

    i64 FileSystem::write_file(u32 inode_number, u64 offset, usize size, const void* buf) {
        Log::debug("[ext4] write_file: inode=%u offset=%llu size=%zu", inode_number, offset, size);

        Inode inode{};
        if (!read_inode(inode_number, inode)) return -1;

        if (inode_get_type(inode) != InodeType::RegularFile) {
            Log::debug("[ext4] write_file: inode=%u is not a regular file", inode_number);
            return -1;
        }

        const u32 bsize = get_block_size();
        auto* block_buf = static_cast<u8*>(kernel::memory::malloc(bsize));
        if (!block_buf) return -1;

        const auto* src = static_cast<const u8*>(buf);
        usize remaining = size;
        u64 cur_off = offset;
        u64 last_phys = 0;  // hint for block allocator locality

        while (remaining > 0) {
            const u32 lblock = static_cast<u32>(cur_off / bsize);
            const u32 block_off = static_cast<u32>(cur_off % bsize);
            const usize chunk = (remaining < bsize - block_off) ? remaining : (bsize - block_off);

            u64 pblock = 0;
            const bool block_exists = map_logical_to_physical(inode, lblock, pblock);

            if (!block_exists) {
                // Allocate a new physical block.
                pblock = alloc_block(last_phys);
                if (pblock == 0) {
                    Log::debug("[ext4] write_file: alloc_block failed at lblock=%u", lblock);
                    kernel::memory::free(block_buf);

                    // Return bytes written so far, or -1 if nothing was written.
                    const usize written = size - remaining;
                    if (written == 0) return -1;
                    // Persist what we have so far before bailing out.
                    write_inode(inode_number, inode);
                    return static_cast<i64>(written);
                }

                // Zero-initialise the newly allocated block so partial writes
                // don't expose stale data.
                memset(block_buf, 0, bsize);
                if (!write_block(pblock, block_buf, bsize)) {
                    kernel::memory::free(block_buf);
                    return -1;
                }

                // Register the new block in the inode's extent tree.
                if (!extent_tree_append(inode, lblock, pblock)) {
                    Log::debug("[ext4] write_file: extent_tree_append failed (tree full?)");
                    kernel::memory::free(block_buf);
                    return -1;
                }

                // Update i_blocks (stored in 512-byte units).
                const u64 blocks_512 =
                    static_cast<u64>(inode.i_blocks_lo) | (static_cast<u64>(inode.i_blocks_high) << 32);
                const u64 new_blocks = blocks_512 + bsize / 512;
                inode.i_blocks_lo = static_cast<u32>(new_blocks & 0xFFFFFFFFu);
                inode.i_blocks_high = static_cast<u16>(new_blocks >> 32);
            }

            // For partial block writes we must read the existing content first
            // so we don't clobber bytes we are not meant to touch.
            if (block_off != 0 || chunk != bsize) {
                if (!read_block(pblock, block_buf, bsize)) {
                    kernel::memory::free(block_buf);
                    return -1;
                }
            }

            memcpy(block_buf + block_off, src, chunk);

            if (!write_block(pblock, block_buf, bsize)) {
                kernel::memory::free(block_buf);
                return -1;
            }

            last_phys = pblock;
            src += chunk;
            cur_off += chunk;
            remaining -= chunk;
        }

        kernel::memory::free(block_buf);

        const u64 new_end = offset + size;
        if (new_end > inode_get_size(inode)) inode_set_size(inode, new_end);

        inode.i_mtime = static_cast<u32>(rtc_to_unix_time());

        if (!write_inode(inode_number, inode)) return -1;

        Log::debug("[ext4] write_file: done, wrote %zu bytes", size);
        return static_cast<i64>(size);
    }

}  // namespace ext4