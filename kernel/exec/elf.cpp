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
#include <kernel/memory.h>

#include "../../filesystem/vfs/vfs.h"
#include <kernel/realm/realm.h>

static uintptr_t align_down(uintptr_t v) { return v & ~(PAGE_SIZE - 1); }
static uintptr_t align_up(uintptr_t v) { return (v + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1); }
inline size_t pages_for(size_t bytes) { return (bytes + PAGE_SIZE - 1) / PAGE_SIZE; }

bool ElfLoader::validate_elf_header(const Elf64_Ehdr* header)
{
    return header->e_ident[0] == 0x7F &&
        header->e_ident[1] == 'E' &&
        header->e_ident[2] == 'L' &&
        header->e_ident[3] == 'F';
}

ElfLoader::ElfFileData ElfLoader::load_file_from_vfs(const char* path)
{
    VfsNode* file = VFS::open(path);
    if (!file)
    {
        return {nullptr, 0, "Failed to open file"};
    }

    size_t size = file->size;
    void* file_data = kernel::memory::malloc(size);

    if (!file_data)
    {
        VFS::close(file);
        return {nullptr, 0, "Failed to allocate memory for file"};
    }

    VFS::read(file, 0, size, file_data);
    VFS::close(file);

    return {file_data, size, nullptr};
}

ElfLoader::ElfLoadResult ElfLoader::validate_elf_file(void* file_data)
{
    if (!file_data)
    {
        return {nullptr, false, "file_data is null"};
    }

    const auto* header = static_cast<Elf64_Ehdr*>(file_data);

    if (!validate_elf_header(header))
    {
        return {nullptr, false, "Invalid ELF file"};
    }

    if (header->e_type != ET_EXEC && header->e_type != ET_DYN)
    {
        return {nullptr, false, "ELF file is not executable"};
    }

    if (header->e_machine != EM_X86_64)
    {
        return {nullptr, false, "ELF file is not for x86_64 architecture"};
    }

    return {nullptr, true, nullptr};
}

ElfLoader::SegmentMapping ElfLoader::calculate_segment_mapping(const Elf64_Phdr& ph, const uintptr_t base_addr)
{
    const uintptr_t seg_vaddr = base_addr + ph.p_vaddr;
    const uintptr_t page_start = align_down(seg_vaddr);
    const size_t page_offset = seg_vaddr - page_start;

    const size_t filesz = ph.p_filesz;
    const size_t memsz = ph.p_memsz;

    const size_t total_needed = page_offset + memsz;
    const size_t map_size = align_up(total_needed);

    return {
        .page_start = page_start,
        .page_offset = page_offset,
        .map_size = map_size,
        .file_size = filesz,
        .memory_size = memsz
    };
}

ElfLoader::ElfLoadResult ElfLoader::map_and_load_segment(
    const Elf64_Phdr& ph,
    const void* file_data,
    uintptr_t base_addr,
    const Realm* r
)
{
    auto [page_start, page_offset, map_size, file_size, memory_size] = calculate_segment_mapping(ph, base_addr);


    void* phys = kernel::memory::request_pages(map_size / PAGE_SIZE);
    if (!phys)
    {
        return {nullptr, false, "Failed to allocate physical memory for segment"};
    }

    uint64_t flags = ph.p_flags;
    flags |= (1ULL << PT_Flag::UserSuper);


    r->page_table->map_range(reinterpret_cast<void*>(page_start),
                             phys,
                             map_size,
                             flags);

    // Daten kopieren
    void* dest = reinterpret_cast<uint8_t*>(page_start) + page_offset;
    memcpy(dest,
           static_cast<const uint8_t*>(file_data) + ph.p_offset,
           file_size);

    // BSS-Bereich nullen (falls memory_size > file_size)
    if (memory_size > file_size)
    {
        memset(static_cast<uint8_t*>(dest) + file_size,
               0,
               memory_size - file_size);
    }

    return {nullptr, true, nullptr};
}

ElfLoader::ElfLoadResult ElfLoader::process_loadable_segments(
    const void* file_data,
    const Elf64_Ehdr* header,
    uintptr_t base_addr,
    const Realm* r
)
{
    if (!file_data)
    {
        return {nullptr, false, "file_data is null"};
    }

    auto* phdrs = reinterpret_cast<const Elf64_Phdr*>(
        static_cast<const uint8_t*>(file_data) + header->e_phoff
    );

    for (int i = 0; i < header->e_phnum; ++i)
    {
        const Elf64_Phdr& ph = phdrs[i];

        if (ph.p_type != PT_LOAD) continue;

        ElfLoadResult segment_result = map_and_load_segment(ph, file_data, base_addr, r);
        if (!segment_result.success)
        {
            return segment_result;
        }
    }

    return {nullptr, true, nullptr};
}

ElfLoader::ElfLoadResult ElfLoader::load_elf_binary(
    const char* path,
    const uintptr_t USERBASE, const Realm* r
)
{
    const ElfFileData file_data = load_file_from_vfs(path);
    if (!file_data.data)
    {
        return {nullptr, false, file_data.error_message};
    }

    ElfLoadResult validation_result = validate_elf_file(file_data.data);
    if (!validation_result.success)
    {
        kernel::memory::free(file_data.data);
        return validation_result;
    }

    const auto* header = static_cast<Elf64_Ehdr*>(file_data.data);
    if (!header)
    {
        kernel::memory::free(file_data.data);
        return {nullptr, false, "ELF header is null"};
    }

    if (const ElfLoadResult load_result = process_loadable_segments(file_data.data, header, USERBASE, r); !load_result.
        success)
    {
        kernel::memory::free(file_data.data);
        return load_result;
    }

    const auto entry_point = reinterpret_cast<UnitEntry>(USERBASE + header->e_entry);
    kernel::memory::free(file_data.data);

    return {entry_point, true, nullptr};
}
