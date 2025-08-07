// elf.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 05.08.25.
//
// This file is part of LuminOS.
// 
// LuminOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// LuminOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with LuminOS. If not, see <https://www.gnu.org/licenses/>.

#include "elf.h"
#include "../../filesystem/vfs/vfs.h"

void *load_elf_binary(const char *path, uint64_t *entry_out) {
    VfsNode *file = vfs_open(path);
    if (!file) {
        Log::Error("Failed to open file %s", path);
        return nullptr;
    };

    size_t size = vfs_file_size(file);
    void *file_data = kernel::memory::malloc(size);
    vfs_read(file, 0, size, file_data);

    auto *header = reinterpret_cast<Elf64_Ehdr *>(file_data);

    // 1. Validieren
    if (header->e_ident[0] != 0x7F || header->e_ident[1] != 'E' ||
        header->e_ident[2] != 'L' || header->e_ident[3] != 'F') {
        Log::Error("Invalid ELF file");
        return nullptr;
    }

    // 2. Programm-Header-Tabelle
    auto *phdrs = reinterpret_cast<Elf64_Phdr *>(
        reinterpret_cast<uint8_t *>(file_data) + header->e_phoff
    );

    for (int i = 0; i < header->e_phnum; ++i) {
        Elf64_Phdr &ph = phdrs[i];

        if (ph.p_type != 1) continue; // PT_LOAD

        // 3. Zieladresse im Virtuellen Speicher
        void *seg_vaddr = reinterpret_cast<void *>(ph.p_vaddr);
        size_t filesz = ph.p_filesz;
        size_t memsz = ph.p_memsz;

        Log::debug("Mapping segment: vaddr = %p, filesz = %d, memsz = %d", seg_vaddr, filesz, memsz);

        // 4. Speicher bereitstellen
        kernel::memory::map_range(seg_vaddr, seg_vaddr, memsz, ph.p_flags); // flags = R/W/X

        // 5. Daten aus dem File in den Speicher kopieren
        memcpy(seg_vaddr,
               reinterpret_cast<uint8_t *>(file_data) + ph.p_offset,
               filesz);

        // 6. BSS (Zero-fill)
        if (memsz > filesz) {
            memset(reinterpret_cast<uint8_t *>(seg_vaddr) + filesz, 0, memsz - filesz);
        }
    }

    // 7. Einstiegspunkt zurückgeben
    *entry_out = header->e_entry;
    return reinterpret_cast<void *>(header->e_entry);
}
