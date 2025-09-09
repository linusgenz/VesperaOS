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
#include <log.h>
#include "../../filesystem/vfs/vfs.h"


bool ElfLoader::validate_elf_header(const Elf64_Ehdr *header) {
    return header->e_ident[0] == 0x7F &&
           header->e_ident[1] == 'E' &&
           header->e_ident[2] == 'L' &&
           header->e_ident[3] == 'F';
}

uint64_t ElfLoader::convert_elf_flags_to_page_flags(uint32_t elf_flags) {
    uint64_t flags = (1ULL << PT_Flag::Present) | (1ULL << PT_Flag::UserSuper);

    if (elf_flags & PF_W) {
        flags |= (1ULL << PT_Flag::ReadWrite);
    }

    if (!(elf_flags & PF_X)) {
        flags |= (1ULL << PT_Flag::NX);
    }

    return flags;
}

Vector<ElfLoader::ElfSegment> ElfLoader::parse_segments(const void *file_data,
                                                        const Elf64_Ehdr *header,
                                                        uintptr_t base_addr) {
    Vector<ElfSegment> segments;

    auto *phdrs = reinterpret_cast<const Elf64_Phdr *>(
        reinterpret_cast<const uint8_t *>(file_data) + header->e_phoff
    );

    for (int i = 0; i < header->e_phnum; ++i) {
        const Elf64_Phdr &ph = phdrs[i];

        if (ph.p_type != PT_LOAD) continue;

        ElfSegment segment = {
            .vaddr = reinterpret_cast<void *>(base_addr + ph.p_vaddr),
            .data_ptr = reinterpret_cast<void *>(
                const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(file_data))
                + ph.p_offset
            ),
            .file_size = ph.p_filesz,
            .memory_size = ph.p_memsz,
            .flags = convert_elf_flags_to_page_flags(ph.p_flags)
        };

        segments.push_back(segment);
    }

    return segments;
}

ElfLoader::ElfLoadResult ElfLoader::load_elf_binary(const char *path, uintptr_t USERBASE, ProcessMemoryManager& mem_manager) {
    VfsNode *file = vfs_open(path);
    if (!file) {
        return {0, false, "Failed to open file"};
    }

    size_t size = vfs_file_size(file);
    void *file_data = kernel::memory::malloc(size);
    if (!file_data) {
        vfs_close(file);
        return {0, false, "Failed to allocate memory for file"};
    }

    vfs_read(file, 0, size, file_data);
    vfs_close(file);

    auto *header = reinterpret_cast<const Elf64_Ehdr *>(file_data);

    if (!validate_elf_header(header)) {
        kernel::memory::free(file_data);
        return {0, false, "Invalid ELF file"};
    }

    auto segments = parse_segments(file_data, header, USERBASE);

    // Load alle Segmente
    for (const auto &segment: segments) {
        uintptr_t start = (uintptr_t) segment.vaddr & ~0xFFFULL;
        uintptr_t end = ((uintptr_t) segment.vaddr + segment.memory_size + 0xFFF) & ~0xFFFULL;
        size_t map_size = end - start;

        if (!mem_manager.map_and_track_range((void*)start, (void*)start,
                                             map_size, segment.flags)) {
            kernel::memory::free(file_data);
            return {0, false, "Failed to map segment"};
                                             }

        memcpy(segment.vaddr, segment.data_ptr, segment.file_size);

        // Zero BSS
        if (segment.memory_size > segment.file_size) {
            memset(reinterpret_cast<uint8_t *>(segment.vaddr) + segment.file_size,
                   0, segment.memory_size - segment.file_size);
        }
    }

    uint64_t entry_point = USERBASE + header->e_entry;
    kernel::memory::free(file_data);

    return {entry_point, true, nullptr};
}
