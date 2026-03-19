/**
 * @file fat32_lfn.cpp
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

#include "fat32_lfn.h"

#include <vespera/mm/memory.h>

namespace fat32 {
    u8 chk_sum(const char* short_name) {
        u8 sum = 0;
        for (int i = 0; i < 11; i++) sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + short_name[i];
        return sum;
    }

    int strcasecmp(const char* s1, const char* s2) {
        while (*s1 && *s2) {
            char c1 = *s1;
            char c2 = *s2;

            // Convert to uppercase
            if (c1 >= 'a' && c1 <= 'z') c1 = c1 - 'a' + 'A';
            if (c2 >= 'a' && c2 <= 'z') c2 = c2 - 'a' + 'A';

            if (c1 != c2) return c1 - c2;

            s1++;
            s2++;
        }

        return *s1 - *s2;
    }

    bool copy_lfn_part(const LongFileName* lfn, char* buffer, usize& pos, const usize max_len) {
        u16 name1[5];
        u16 name2[6];
        u16 name3[2];
        memcpy(name1, lfn->name1, sizeof(name1));
        memcpy(name2, lfn->name2, sizeof(name2));
        memcpy(name3, lfn->name3, sizeof(name3));

        auto copy_chars = [&](const u16* src, const usize count) {
            for (usize i = 0; i < count; i++) {
                if (src[i] == 0x0000 || src[i] == 0xFFFF) {
                    return false;
                }
                if (pos >= max_len - 1) {
                    return false;
                }
                buffer[pos++] = static_cast<char>(src[i] & 0xFF);
            }
            return true;
        };

        if (!copy_chars(name1, 5)) return false;
        if (!copy_chars(name2, 6)) return false;
        if (!copy_chars(name3, 2)) return false;

        return true;
    }

    bool make_short_name(const char* input, char* output11) {
        memset(output11, ' ', 11);
        const char* dot = strrchr(input, '.');
        const usize name_len = dot ? static_cast<usize>(dot - input) : strlen(input);
        const usize ext_len = dot ? strlen(dot + 1) : 0;
        if (name_len == 0) return false;

        usize out_pos = 0;
        for (usize i = 0; i < name_len && out_pos < 8;
            i++) {
            const char c = input[i];
            if (c == ' ' || c == '.' || c == '+' || c == ',' || c == ';') continue;
            output11[out_pos++] = to_upper(c);
        }

        if (name_len <= 8) {

            output11[6] = '~';
            output11[7] = '1';
        }

        if (dot && ext_len > 0) {
            usize ext_pos = 0;
            for (usize i = 0; i < 3 && dot[1 + i]; i++) {
                const char c = dot[1 + i];
                if (c == ' ' || c == '.' || c == '+' || c == ',' || c == ';') continue;
                output11[8 + ext_pos++] = to_upper(c);
            }
        }

        return true;
    }

    void extract_short_name(const unsigned char* raw_name, char* short_name_buffer, const usize buffer_size) {
        if (buffer_size < 13) return;

        // Name Teil (8 Zeichen) - IMMER UPPERCASE
        usize pos = 0;
        for (int i = 0; i < 8; i++) {
            if (raw_name[i] != ' ') short_name_buffer[pos++] = raw_name[i];
        }

        // Extension Teil (3 Zeichen) - IMMER UPPERCASE
        bool has_ext = false;
        for (int i = 8; i < 11; i++) {
            if (raw_name[i] != ' ') {
                has_ext = true;
                break;
            }
        }

        if (has_ext) {
            short_name_buffer[pos++] = '.';
            for (int i = 8; i < 11; i++) {
                if (raw_name[i] != ' ') short_name_buffer[pos++] = raw_name[i];
            }
        }

        short_name_buffer[pos] = '\0';
    }

    bool write_lfn_entries(
        DirectoryEntry* entries, const usize start_index, const char* long_name, const char* short_name,
        const usize name_len
    ) {
        const usize entries_needed = (name_len + 12) / 13;
        u16 name_buffer[256] = {};

        for (usize j = 0; j < name_len; ++j) name_buffer[j] = static_cast<u8>(long_name[j]);

        const u8 checksum = chk_sum(short_name);

        for (int lfn_index = static_cast<int>(entries_needed) - 1; lfn_index >= 0; --lfn_index) {
            LongFileName lfn = {};
            lfn.order = static_cast<u8>(lfn_index + 1);
            if (lfn_index == static_cast<int>(entries_needed) - 1) lfn.order |= 0x40;

            lfn.attr = ATTR_LONG_NAME;
            lfn.type = 0;
            lfn.checksum = checksum;
            lfn.first_cluster_low = 0;

            usize name_pos = static_cast<usize>(lfn_index) * 13;

            u16 tmp_name1[5] = {};
            u16 tmp_name2[6] = {};
            u16 tmp_name3[2] = {};

            auto copy_from_name = [&](u16* dest, const int count) {
                for (int c = 0; c < count; ++c) {
                    if (name_pos < name_len)
                        dest[c] = name_buffer[name_pos++];
                    else if (name_pos == name_len) {
                        dest[c] = 0x0000;
                        name_pos++;
                    } else
                        dest[c] = 0xFFFF;
                }
            };

            copy_from_name(tmp_name1, 5);
            copy_from_name(tmp_name2, 6);
            copy_from_name(tmp_name3, 2);

            memcpy(lfn.name1, tmp_name1, sizeof(tmp_name1));
            memcpy(lfn.name2, tmp_name2, sizeof(tmp_name2));
            memcpy(lfn.name3, tmp_name3, sizeof(tmp_name3));

            memcpy(
                &entries[start_index + (entries_needed - 1 - static_cast<usize>(lfn_index))], &lfn, sizeof(LongFileName)
            );
        }

        return true;
    }

    usize find_first_lfn_index(const FileEntry* entries, const usize short_name_index) {
        if (short_name_index == 0) return short_name_index;

        usize first_lfn = short_name_index;
        for (int i = static_cast<int>(short_name_index) - 1; i >= 0; i--) {
            if (const DirectoryEntry entry = entries[i].get_directory_entry(); entry.attr == ATTR_LONG_NAME)
                first_lfn = i;
            else
                break;
        }

        return first_lfn;
    }
}  // namespace FAT32