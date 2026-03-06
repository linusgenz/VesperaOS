/**
 * @file fat32_lfn.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 06.01.26.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef VESPERAOS_FAT32_LFN_H
#define VESPERAOS_FAT32_LFN_H

#include "fat32.h"

namespace fat32
{
    uint8_t chk_sum(const char* short_name);
    bool copy_lfn_part(const LongFileName* lfn, char* buffer, size_t& pos, size_t max_len);
    bool make_short_name(const char* input, char* output11);
    void extract_short_name(const unsigned char* raw_name, char* short_name_buffer, size_t buffer_size);


    bool write_lfn_entries(DirectoryEntry* entries, size_t start_index,
                         const char* long_name, const char* short_name,
                         size_t name_len);


    size_t find_first_lfn_index(const FileEntry* entries, size_t short_name_index);
}

#endif //VESPERAOS_FAT32_LFN_H