// partition.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 30.09.25.
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

#include "partition.h"

#include <vespera/devices/block.h>
#include <klib/encoding.h>
#include <klib/string.h>
#include <vespera/mm/memory.h>

inline u16 rd16(const void *p) {
    return *static_cast<const u16*>(p);
}

inline u32 rd32(const void *p) {
    return *static_cast<const u32*>(p);
}

inline u64 rd64(const void *p) {
    return *static_cast<const u64*>(p);
}


usize parse_partitions(BlockDevice *device, PartitionEntry *out, usize max_entries) {
    if (!device || !out || max_entries == 0) return 0;

    u8 sector[512];

    // try GPT header first (LBA 1)
    if (device->read(1, 1, sector, sizeof(sector))) {
        if (memcmp(sector, "EFI PART", 8) == 0) {
            u64 part_lba = rd64(sector + 72);       // partition_entry_lba
            u32 part_count = rd32(sector + 80);     // num_partition_entries
            u32 part_size  = rd32(sector + 84);     // size_of_partition_entry

            usize added = 0;
            u8 entrybuf[512];

            for (u32 i = 0; i < part_count && added < max_entries; ++i) {
                u64 entry_index = static_cast<u64>(i) * part_size;
                u64 sector_idx = part_lba + (entry_index / 512);
                u32 offset_in_sector = entry_index % 512;

                if (!device->read(sector_idx, 1, entrybuf, sizeof(entrybuf))) break;

                const u8 *entry = entrybuf + offset_in_sector;

                // Partition type GUID all zero => unused
                bool all_zero = true;
                for (int j = 0; j < 16; ++j) if (entry[j]) { all_zero = false; break; }
                if (all_zero) {
                    // continue to next entry (some entries cross sector boundary - handle basic case)
                    if (offset_in_sector + part_size > 512) {
                        // read next sector to cover full entry (simple handling)
                        u8 tmp[512];
                        if (!device->read(sector_idx + 1, 1, tmp, sizeof(tmp))) break;
                        // copy combined bytes into a small buffer
                        u8 full[128]; // part_size is usually 128
                        usize first = 512 - offset_in_sector;
                        memcpy(full, entrybuf + offset_in_sector, first);
                        memcpy(full + first, tmp, part_size - first);
                        entry = full;
                    } else {
                        continue;
                    }
                }

                u64 start = rd64(entry + 32);
                u64 end   = rd64(entry + 40);
                if (start == 0 && end == 0) continue;

                out[added].start_lba = start;
                out[added].length_lba = (end >= start) ? (end - start + 1) : 0;
                out[added].mbr_type = 0;

                const auto *utf16_name = reinterpret_cast<const utf16_t*>(entry + 56);
                char name_buf[72];
                utf16_to_utf8(utf16_name, 36, name_buf);

                strncpy(out[added].name, name_buf, sizeof(out[added].name)-1);
                out[added].name[sizeof(out[added].name)-1] = '\0';
                ++added;
            }

            return added;
        }
    }

    // Fallback: MBR (LBA 0)
    if (!device->read(0, 1, sector, sizeof(sector))) return 0;
    // check signature 0x55AA
    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        return 0;
    }

    usize added = 0;
    for (int i = 0; i < 4 && added < max_entries; ++i) {
        constexpr int mbr_part_off = 446;
        const u8 *e = sector + mbr_part_off + i * 16;
        u8 type = e[4];
        u32 start = rd32(e + 8);
        u32 len   = rd32(e + 12);
        if (type == 0 || len == 0) continue;

        out[added].start_lba = start;
        out[added].length_lba = len;
        out[added].mbr_type = type;
        out[added].name[0] = '\0';
        ++added;
    }

    return added;
}