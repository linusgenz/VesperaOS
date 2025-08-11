// elf.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 05.08.25.
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

#include "elf.h"
#include "../../filesystem/vfs/vfs.h"


void *load_elf_binary(const char *path, uint64_t *entry_out, uintptr_t USER_BASE) {
    VfsNode *file = vfs_open(path);
    if (!file) {
        Log::Error("Failed to open file %s", path);
        return nullptr;
    };

    size_t size = vfs_file_size(file);
    void *file_data = kernel::memory::malloc(size);
    vfs_read(file, 0, size, file_data);

    auto *header = reinterpret_cast<Elf64_Ehdr *>(file_data);

    if (header->e_ident[0] != 0x7F || header->e_ident[1] != 'E' ||
        header->e_ident[2] != 'L' || header->e_ident[3] != 'F') {
        Log::Error("Invalid ELF file");
        return nullptr;
    }

    auto *phdrs = reinterpret_cast<Elf64_Phdr *>(
        reinterpret_cast<uint8_t *>(file_data) + header->e_phoff
    );

    for (int i = 0; i < header->e_phnum; ++i) {
        Elf64_Phdr &ph = phdrs[i];

        if (ph.p_type != 1) continue; // PT_LOAD

        // 3. Zieladresse im Virtuellen Speicher
        void *seg_vaddr = reinterpret_cast<void *>(USER_BASE + ph.p_vaddr);
        size_t filesz = ph.p_filesz;
        size_t memsz = ph.p_memsz;

        Log::debug("Mapping segment: vaddr = %p, filesz = %d, memsz = %d", seg_vaddr, filesz, memsz);

        kernel::memory::map_range(seg_vaddr, seg_vaddr, memsz, ph.p_flags); // flags = R/W/X

        memcpy(seg_vaddr,
               reinterpret_cast<uint8_t *>(file_data) + ph.p_offset,
               filesz);

        //  zeroing bss
        if (memsz > filesz) {
            memset(reinterpret_cast<uint8_t *>(seg_vaddr) + filesz, 0, memsz - filesz);
        }
    }

    *entry_out = USER_BASE + header->e_entry;
    return reinterpret_cast<void *>(header->e_entry);
}
