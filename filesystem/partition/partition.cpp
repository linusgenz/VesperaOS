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

#include "../kernel/devices/blockdevice.h"
#include "partition.h"

#include <encoding.h>
#include <string.h>
#include <kernel/memory.h>

inline uint16_t rd16(const void *p) {
    return *static_cast<const uint16_t*>(p);
}

inline uint32_t rd32(const void *p) {
    return *static_cast<const uint32_t*>(p);
}

inline uint64_t rd64(const void *p) {
    return *static_cast<const uint64_t*>(p);
}


size_t parse_partitions(BlockDevice *device, PartitionEntry *out, size_t max_entries) {
    if (!device || !out || max_entries == 0) return 0;

    uint8_t sector[512];

    // try GPT header first (LBA 1)
    if (device->read(1, 1, sector, sizeof(sector))) {
        if (memcmp(sector, "EFI PART", 8) == 0) {
            uint64_t part_lba = rd64(sector + 72);       // partition_entry_lba
            uint32_t part_count = rd32(sector + 80);     // num_partition_entries
            uint32_t part_size  = rd32(sector + 84);     // size_of_partition_entry

            size_t added = 0;
            uint8_t entrybuf[512];

            for (uint32_t i = 0; i < part_count && added < max_entries; ++i) {
                uint64_t entry_index = static_cast<uint64_t>(i) * part_size;
                uint64_t sector_idx = part_lba + (entry_index / 512);
                uint32_t offset_in_sector = entry_index % 512;

                if (!device->read(sector_idx, 1, entrybuf, sizeof(entrybuf))) break;

                const uint8_t *entry = entrybuf + offset_in_sector;

                // Partition type GUID all zero => unused
                bool all_zero = true;
                for (int j = 0; j < 16; ++j) if (entry[j]) { all_zero = false; break; }
                if (all_zero) {
                    // continue to next entry (some entries cross sector boundary - handle basic case)
                    if (offset_in_sector + part_size > 512) {
                        // read next sector to cover full entry (simple handling)
                        uint8_t tmp[512];
                        if (!device->read(sector_idx + 1, 1, tmp, sizeof(tmp))) break;
                        // copy combined bytes into a small buffer
                        uint8_t full[128]; // part_size is usually 128
                        size_t first = 512 - offset_in_sector;
                        memcpy(full, entrybuf + offset_in_sector, first);
                        memcpy(full + first, tmp, part_size - first);
                        entry = full;
                    } else {
                        continue;
                    }
                }

                uint64_t start = rd64(entry + 32);
                uint64_t end   = rd64(entry + 40);
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

    size_t added = 0;
    for (int i = 0; i < 4 && added < max_entries; ++i) {
        constexpr int mbr_part_off = 446;
        const uint8_t *e = sector + mbr_part_off + i * 16;
        uint8_t type = e[4];
        uint32_t start = rd32(e + 8);
        uint32_t len   = rd32(e + 12);
        if (type == 0 || len == 0) continue;

        out[added].start_lba = start;
        out[added].length_lba = len;
        out[added].mbr_type = type;
        out[added].name[0] = '\0';
        ++added;
    }

    return added;
}