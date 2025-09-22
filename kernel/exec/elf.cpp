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

static inline uintptr_t align_down(uintptr_t v) { return v & ~(PAGE_SIZE - 1); }
static inline uintptr_t align_up(uintptr_t v) { return (v + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1); }
inline size_t pages_for(size_t bytes) { return (bytes + PAGE_SIZE - 1) / PAGE_SIZE; }

bool ElfLoader::validate_elf_header(const Elf64_Ehdr *header) {
    return header->e_ident[0] == 0x7F &&
           header->e_ident[1] == 'E' &&
           header->e_ident[2] == 'L' &&
           header->e_ident[3] == 'F';
}

ElfLoader::ElfFileData ElfLoader::load_file_from_vfs(const char *path) {
    VfsNode *file = vfs_open(path);
    if (!file) {
        return {nullptr, 0, "Failed to open file"};
    }

    size_t size = vfs_file_size(file);
    void *file_data = kernel::memory::malloc(size);

    if (!file_data) {
        vfs_close(file);
        return {nullptr, 0, "Failed to allocate memory for file"};
    }

    vfs_read(file, 0, size, file_data);
    vfs_close(file);

    return {file_data, size, nullptr};
}

ElfLoader::ElfLoadResult ElfLoader::validate_elf_file(void *file_data) {
    auto *header = reinterpret_cast<Elf64_Ehdr *>(file_data);

    if (!validate_elf_header(header)) {
        return {0, false, "Invalid ELF file"};
    }

    if (header->e_type != ET_EXEC && header->e_type != ET_DYN) {
        return {0, false, "ELF file is not executable"};
    }

    if (header->e_machine != EM_X86_64) {
        return {0, false, "ELF file is not for x86_64 architecture"};
    }

    return {0, true, nullptr};
}

ElfLoader::SegmentMapping ElfLoader::calculate_segment_mapping(const Elf64_Phdr &ph, uintptr_t base_addr) {
    uintptr_t seg_vaddr = base_addr + ph.p_vaddr;
    uintptr_t page_start = align_down(seg_vaddr);
    size_t page_offset = static_cast<size_t>(seg_vaddr - page_start);

    size_t filesz = static_cast<size_t>(ph.p_filesz);
    size_t memsz = static_cast<size_t>(ph.p_memsz);

    size_t total_needed = page_offset + memsz;
    size_t map_size = align_up(total_needed);

    return {
        .page_start = page_start,
        .page_offset = page_offset,
        .map_size = map_size,
        .file_size = filesz,
        .memory_size = memsz
    };
}

ElfLoader::ElfLoadResult ElfLoader::map_and_load_segment(
    const Elf64_Phdr &ph,
    const void *file_data,
    uintptr_t base_addr
) {
    SegmentMapping mapping = calculate_segment_mapping(ph, base_addr);


    void *phys = kernel::memory::request_pages(mapping.map_size / PAGE_SIZE);
    if (!phys) {
        return {0, false, "Failed to allocate physical memory for segment"};
    }

    uint64_t flags = ph.p_flags;
    flags |= (1ULL << PT_Flag::UserSuper);


    kernel::memory::map_range(reinterpret_cast<void *>(mapping.page_start),
                              phys,
                              mapping.map_size,
                              flags);

    // Daten kopieren
    void *dest = reinterpret_cast<uint8_t *>(mapping.page_start) + mapping.page_offset;
    memcpy(dest,
           reinterpret_cast<const uint8_t *>(file_data) + ph.p_offset,
           mapping.file_size);

    // BSS-Bereich nullen (falls memory_size > file_size)
    if (mapping.memory_size > mapping.file_size) {
        memset(reinterpret_cast<uint8_t *>(dest) + mapping.file_size,
               0,
               mapping.memory_size - mapping.file_size);
    }

    return {0, true, nullptr};
}

ElfLoader::ElfLoadResult ElfLoader::process_loadable_segments(
    const void *file_data,
    const Elf64_Ehdr *header,
    uintptr_t base_addr
) {
    auto *phdrs = reinterpret_cast<const Elf64_Phdr *>(
        reinterpret_cast<const uint8_t *>(file_data) + header->e_phoff
    );

    for (int i = 0; i < header->e_phnum; ++i) {
        const Elf64_Phdr &ph = phdrs[i];

        if (ph.p_type != PT_LOAD) continue;

        ElfLoadResult segment_result = map_and_load_segment(ph, file_data, base_addr);
        if (!segment_result.success) {
            return segment_result;
        }
    }

    return {0, true, nullptr};
}

ElfLoader::ElfLoadResult ElfLoader::load_elf_binary(
    const char *path,
    uintptr_t USER_BASE
) {
    ElfFileData file_data = load_file_from_vfs(path);
    if (!file_data.data) {
        return {0, false, file_data.error_message};
    }

    ElfLoadResult validation_result = validate_elf_file(file_data.data);
    if (!validation_result.success) {
        kernel::memory::free(file_data.data);
        return validation_result;
    }

    const auto *header = reinterpret_cast<Elf64_Ehdr *>(file_data.data);

    ElfLoadResult load_result = process_loadable_segments(file_data.data, header, USER_BASE);
    if (!load_result.success) {
        kernel::memory::free(file_data.data);
        return load_result;
    }

    uintptr_t entry_point = USER_BASE + header->e_entry;
    kernel::memory::free(file_data.data);

    return {entry_point, true, nullptr};
}
