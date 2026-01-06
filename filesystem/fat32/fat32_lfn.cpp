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

namespace FAT32
{
    uint8_t ChkSum(const char* shortName)
    {
        uint8_t sum = 0;
        for (int i = 0; i < 11; i++)
            sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + shortName[i];
        return sum;
    }

    bool CopyLFNPart(const LongFileName* lfn, char* buffer, size_t& pos, size_t maxLen)
    {
        auto copyChars = [&](const uint16_t* src, size_t count)
        {
            for (size_t i = 0; i < count; i++)
            {
                if (src[i] == 0x0000 || src[i] == 0xFFFF) return false;
                if (pos >= maxLen - 1) return false;
                buffer[pos++] = static_cast<char>(src[i] & 0xFF);
            }
            return true;
        };
        return copyChars(lfn->name1, 5) &&
               copyChars(lfn->name2, 6) &&
               copyChars(lfn->name3, 2);
    }

    bool MakeShortName(const char* input, char* output11)
    {
        memset(output11, ' ', 11);
        const char* dot = strrchr(input, '.');
        size_t nameLen = dot ? static_cast<size_t>(dot - input) : strlen(input);
        size_t extLen = dot ? strlen(dot + 1) : 0;
        if (nameLen == 0) return false;

        size_t outPos = 0;
        for (size_t i = 0; i < nameLen && outPos < 8; i++)
        {
            char c = input[i];
            if (c == ' ' || c == '.' || c == '+' || c == ',' || c == ';') continue;
            output11[outPos++] = to_upper(c);
        }

        if (nameLen > 8)
        {
            output11[6] = '~';
            output11[7] = '1';
        }

        if (dot && extLen > 0)
        {
            size_t extPos = 0;
            for (size_t i = 0; i < 3 && dot[1 + i]; i++)
            {
                char c = dot[1 + i];
                if (c == ' ' || c == '.' || c == '+' || c == ',' || c == ';') continue;
                output11[8 + extPos++] = to_upper(c);
            }
        }

        return true;
    }

    void ExtractShortName(const unsigned char* rawName, char* shortNameBuffer, size_t bufferSize)
    {
        if (bufferSize < 13) return;
        memcpy(shortNameBuffer, rawName, 11);
        for (int i = 10; i >= 0; i--)
        {
            if (shortNameBuffer[i] == ' ')
                shortNameBuffer[i] = '\0';
            else
                break;
        }
        shortNameBuffer[12] = '\0';
    }

    bool WriteLFNEntries(DirectoryEntry* entries, size_t startIndex,
                         const char* longName, const char* shortName,
                         size_t nameLen)
    {
        uint16_t nameBuffer[256] = {};
        for (size_t j = 0; j < nameLen; ++j)
            nameBuffer[j] = static_cast<uint8_t>(longName[j]);

        const size_t entriesNeeded = (nameLen + 12) / 13;
        uint8_t checksum = ChkSum(shortName);

        for (int lfnIndex = static_cast<int>(entriesNeeded) - 1; lfnIndex >= 0; --lfnIndex)
        {
            LongFileName lfn{};
            lfn.order = static_cast<uint8_t>(lfnIndex + 1);
            if (lfnIndex == static_cast<int>(entriesNeeded) - 1) lfn.order |= 0x40;
            lfn.attr = ATTR_LONG_NAME;
            lfn.type = 0;
            lfn.checksum = checksum;
            lfn.firstClusterLow = 0;

            size_t namePos = static_cast<size_t>(lfnIndex) * 13;
            auto copy_from_name = [&](uint16_t* dest, int count)
            {
                for (int c = 0; c < count; ++c)
                {
                    if (namePos < nameLen) dest[c] = nameBuffer[namePos++];
                    else if (namePos == nameLen) { dest[c] = 0x0000; namePos++; }
                    else dest[c] = 0xFFFF;
                }
            };
            copy_from_name(lfn.name1, 5);
            copy_from_name(lfn.name2, 6);
            copy_from_name(lfn.name3, 2);

            memcpy(&entries[startIndex + lfnIndex], &lfn, sizeof(LongFileName));
        }

        return true;
    }

    size_t FindFirstLFNIndex(const FileEntry* entries, size_t shortNameIndex)
    {
        if (shortNameIndex == 0) return shortNameIndex;

        size_t firstLFN = shortNameIndex;
        for (int i = static_cast<int>(shortNameIndex) - 1; i >= 0; i--)
        {
            DirectoryEntry entry = entries[i].GetDirectoryEntry();
            if (entry.attr == ATTR_LONG_NAME) firstLFN = i;
            else break;
        }

        return firstLFN;
    }
}
