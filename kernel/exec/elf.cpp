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

/*
ElfLoader::ElfLoadResult ElfLoader::load_elf_binary(const char *path,
                                                    uintptr_t offset) {
    VfsNode *file = vfs_open(path);
    if (!file) return {0, false, "Failed to open file"};

    size_t size = vfs_file_size(file);
    void *file_data = kernel::memory::malloc(size);
    vfs_read(file, 0, size, file_data);
    vfs_close(file);

    auto *header = reinterpret_cast<const Elf64_Ehdr *>(file_data);
    if (!validate_elf_header(header)) {
        kernel::memory::free(file_data);
        return {0, false, "Invalid ELF"};
    }

    auto segments = parse_segments(file_data, header, offset);

    for (auto &seg: segments) {
        uintptr_t start = align_down((uintptr_t) seg.vaddr);
        uintptr_t end = align_up((uintptr_t) seg.vaddr + seg.memory_size);
        size_t map_size = end - start;

        // 1. Physische Frames holen
        void *phys = kernel::memory::request_pages(map_size / PAGE_SIZE);
        if (!phys) {
            kernel::memory::free(file_data);
            return {0, false, "Out of memory"};
        }

        Log::LogMsg("start %p", start);
        Log::LogMsg("phys %p", phys);
        kernel::memory::map_range((void *) start, phys, map_size, seg.flags);

        // 3. ELF-Inhalt kopieren (in virt==phys)
        memcpy(phys, seg.data_ptr, seg.file_size);

        if (seg.memory_size > seg.file_size) {
            memset((uint8_t *) phys + seg.file_size, 0,
                   seg.memory_size - seg.file_size);
        }
    }

    uint64_t entry_point = offset + header->e_entry; // direkt aus ELF
    kernel::memory::free(file_data);
    return {entry_point, true, nullptr};
}*/

ElfLoader::ElfLoadResult ElfLoader::load_elf_binary(const char *path, uintptr_t USER_BASE) {
    VfsNode *file = vfs_open(path);
    if (!file) {
        return {0, false, "Failed to open file"};
    };

    size_t size = vfs_file_size(file);
    void *file_data = kernel::memory::malloc(size);
    vfs_read(file, 0, size, file_data);

    auto *header = reinterpret_cast<Elf64_Ehdr *>(file_data);

    if (header->e_ident[0] != 0x7F || header->e_ident[1] != 'E' ||
        header->e_ident[2] != 'L' || header->e_ident[3] != 'F') {
        return {0, false, "Invalid ELF file"};
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

        kernel::memory::map_range(seg_vaddr, seg_vaddr, memsz, ph.p_flags); // flags = R/W/X

        memcpy(seg_vaddr,
               reinterpret_cast<uint8_t *>(file_data) + ph.p_offset,
               filesz);

        //  zeroing bss
        if (memsz > filesz) {
            memset(reinterpret_cast<uint8_t *>(seg_vaddr) + filesz, 0, memsz - filesz);
        }
    }
    return {USER_BASE + header->e_entry, true, nullptr};
}
