// partition.h
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

#ifndef VESPERAOS_PARTITION_H
#define VESPERAOS_PARTITION_H

#include <stddef.h>

#define PARTITION_MAX_ENTRIES 128

struct PartitionEntry {
    uint64_t start_lba;
    uint64_t length_lba;
    uint64_t sector_size;
    uint8_t mbr_type;  // for MBR partitions (0 == unused). For GPT may be 0.
    char name[72];     // GPT name (in utf-8)
};

size_t parse_partitions(BlockDevice *device, PartitionEntry *out, size_t max_entries);

#endif  // VESPERAOS_PARTITION_H