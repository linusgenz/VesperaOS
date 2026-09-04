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

#include <filesystem/vfs.h>
#include <klib/string.h>
#include <realm/realm.h> // TODO TEMPORARY REMOVE WHEN REFACTORED
#include <vespera/mm/memory.h>

#include "../paging/page_table_manager.h"
#include "../realm/address_space.h"
#include "../security/setuid_exec.h"
#include "vespera/log.h"
#include "vespera/debug/chronos.h"

#if ENABLE_ELF_LOGGING
#include <vespera/log.h>
#define ELF_LOG(fmt, ...) Log::info(fmt, ##__VA_ARGS__)
#else
#define ELF_LOG(fmt, ...)
#endif

/** @brief Fixed load base for the dynamic linker (PT_INTERP), well clear of any PIE main image. */
static constexpr uptr INTERP_LOAD_BASE = 0x40000000ULL;

static phys_addr_t realm_get_phys(const Realm* realm, const uptr vaddr) {
    const uptr page_vaddr = vaddr & ~0xFFFULL;
    const uptr offset = vaddr & 0xFFFULL;

    const phys_addr_t phys_page = realm->address_space->page_table()->get_physical_address(virt_from_raw(page_vaddr));
    if (phys_null(phys_page)) return make_phys(0);

    return phys_add(phys_page, offset);
}

static bool find_interp_path(const Elf64_Ehdr* header, const void* file_data, char* out_path, const usize out_size) {
    auto* phdrs = reinterpret_cast<const Elf64_Phdr*>(static_cast<const u8*>(file_data) + header->e_phoff);

    for (int i = 0; i < header->e_phnum; ++i) {
        const Elf64_Phdr& ph = phdrs[i];
        if (ph.p_type != PT_INTERP) continue;

        if (ph.p_filesz == 0 || ph.p_filesz >= out_size) return false;

        memcpy(out_path, static_cast<const u8*>(file_data) + ph.p_offset, ph.p_filesz);
        out_path[ph.p_filesz] = '\0';
        return true;
    }

    return false;
}

/** @brief Builds a failed @ref ElfLoader::LoadResult, defaulting all interpreter fields to empty. */
static ElfLoader::LoadResult make_load_error(const char* message, const bool is_pie = false) {
    return {
        .entry_point = nullptr,
        .load_base = 0,
        .load_end = 0,
        .vaddr_base = 0,
        .load_bias = 0,
        .success = false,
        .error_message = message,
        .is_pie = is_pie,
        .has_interp = false,
        .interp_entry = nullptr,
        .interp_base = 0,
        .phdr_vaddr = 0,
        .phent = 0,
        .phnum = 0
    };
}

ElfLoader::LoadResult ElfLoader::load(const char* path, const uptr preferred_base, Realm* realm) {
    return load_internal(path, preferred_base, realm, true);
}

ElfLoader::LoadResult ElfLoader::load_internal(
    const char* path, const uptr preferred_base, Realm* realm, const bool resolve_interp
) {
    if (!path || !realm) {
        return make_load_error("Invalid parameters: path or realm is null");
    }

    ELF_LOG("[ELF] Loading binary from: %s", path);

    const FileData file_data = load_file_from_vfs(path, realm);
    if (!file_data.data) {
        return make_load_error(file_data.error_message);
    }

    const auto* header = static_cast<Elf64_Ehdr*>(file_data.data);

    if (!validate_magic(header)) {
        kernel::memory::free(file_data.data);
        Log::debug("%x %x %x", header->e_ident[0], header->e_ident[1], header->e_ident[2]);
        return make_load_error("Invalid ELF magic bytes");
    }

    if (!validate_type(header)) {
        kernel::memory::free(file_data.data);
        return make_load_error("Invalid ELF type (must be ET_EXEC or ET_DYN)");
    }

    if (!validate_architecture(header)) {
        kernel::memory::free(file_data.data);
        return make_load_error("Invalid architecture (must be x86_64)");
    }

    const bool is_pie = (header->e_type == ET_DYN);
    ELF_LOG("[ELF] Type: %s, Entry: 0x%lx", is_pie ? "ET_DYN (PIE)" : "ET_EXEC", header->e_entry);

    // PT_INTERP must be resolved before segments are loaded, since the interpreter is loaded
    // as a fully separate image (own address range, own bias) via a recursive load_internal call.
    char interp_path[256];
    const bool has_interp = resolve_interp && find_interp_path(header, file_data.data, interp_path, sizeof(interp_path));

    AddressRange range{};
    if (!calculate_address_range(header, file_data.data, range)) {
        kernel::memory::free(file_data.data);
        return make_load_error("No PT_LOAD segments found");
    }

    ELF_LOG("[ELF] Virtual range: 0x%lx - 0x%lx (size: %lu)", range.vaddr_min, range.vaddr_max, range.total_size);

    const uptr load_bias = calculate_load_bias(header, range, preferred_base);
    const uptr load_base = range.vaddr_min + load_bias;

    ELF_LOG("[ELF] Load base: 0x%lx, Bias: 0x%lx", load_base, load_bias);

    if (!process_all_segments(header, file_data.data, load_bias, realm)) {
        kernel::memory::free(file_data.data);
        return make_load_error("Failed to load segments", is_pie);
    }

    const TlsInfo tls = find_tls_segment(header, file_data.data);
    if (tls.present) {
        void* tmpl = kernel::memory::malloc(tls.file_size);
        if (tls.file_size > 0)
            memcpy(tmpl, static_cast<const u8*>(file_data.data) + tls.file_offset, tls.file_size);

        realm->tls_template = {
            .init_data = tmpl,
            .file_size = tls.file_size,
            .mem_size  = tls.mem_size,
            .align     = tls.align ? tls.align : 1,
            .present   = true,
        };
    }

    if (is_pie) {
        if (!apply_relocations(header, file_data.data, load_bias, realm)) {
            kernel::memory::free(file_data.data);
            return make_load_error("Failed to apply relocations", is_pie);
        }
    }

    const uptr entry_point = header->e_entry + load_bias;

    const uptr phdr_vaddr = header->e_phoff + load_bias;

    const uptr load_end = align_up(range.vaddr_max + load_bias, PAGE_SIZE);

    ELF_LOG("[ELF] Entry point: 0x%lx -> 0x%lx (relocated)", header->e_entry, entry_point);
    ELF_LOG("[ELF] Loaded range: 0x%lx - 0x%lx", load_base, load_end);

    const u16 phent = header->e_phentsize;
    const u16 phnum = header->e_phnum;

    kernel::memory::free(file_data.data);

    if (has_interp) {
        const LoadResult interp = load_internal(interp_path, INTERP_LOAD_BASE, realm, false);
        if (!interp.success) {
            return make_load_error("Failed to load PT_INTERP. Is the specified linker present?", is_pie);
        }

        return {
            .entry_point = reinterpret_cast<void*>(entry_point),
            .load_base = load_base,
            .load_end = load_end,
            .vaddr_base = range.vaddr_min,
            .load_bias = load_bias,
            .success = true,
            .error_message = nullptr,
            .is_pie = is_pie,
            .has_interp = true,
            .interp_entry = interp.entry_point,
            .interp_base = interp.load_base,
            .phdr_vaddr = phdr_vaddr,
            .phent = phent,
            .phnum = phnum
        };
    }

    return {
        .entry_point = reinterpret_cast<void*>(entry_point),
        .load_base = load_base,
        .load_end = load_end,
        .vaddr_base = range.vaddr_min,
        .load_bias = load_bias,
        .success = true,
        .error_message = nullptr,
        .is_pie = is_pie,
        .has_interp = false,
        .interp_entry = nullptr,
        .interp_base = 0,
        .phdr_vaddr = phdr_vaddr,
        .phent = phent,
        .phnum = phnum
    };
}

bool ElfLoader::apply_relocations(
    const Elf64_Ehdr* header, const void* file_data, const uptr load_bias, const Realm* realm
) {
    auto* phdrs = reinterpret_cast<const Elf64_Phdr*>(static_cast<const u8*>(file_data) + header->e_phoff);

    const Elf64_Phdr* dynamic_phdr = nullptr;
    for (int i = 0; i < header->e_phnum; ++i) {
        if (phdrs[i].p_type == PT_DYNAMIC) {
            dynamic_phdr = &phdrs[i];
            break;
        }
    }
    if (!dynamic_phdr) return true;

    auto* dyn = reinterpret_cast<const Elf64_Dyn*>(static_cast<const u8*>(file_data) + dynamic_phdr->p_offset);

    const usize dyn_count = dynamic_phdr->p_filesz / sizeof(Elf64_Dyn);

    usize rela_sz = 0;
    usize rela_ent = sizeof(Elf64_Rela);
    uptr rela_vaddr = 0;

    for (usize i = 0; i < dyn_count; ++i) {
        switch (dyn[i].d_tag) {
            case DT_RELA:
                rela_vaddr = dyn[i].d_un.d_ptr;
                break;
            case DT_RELASZ:
                rela_sz = dyn[i].d_un.d_val;
                break;
            case DT_RELAENT:
                rela_ent = dyn[i].d_un.d_val;
                break;
            case DT_NULL:
                goto done_dyn;
            default:
                break;
        }
    }
done_dyn:

    if (!rela_vaddr || rela_sz == 0) return true;

    const Elf64_Rela* rela = nullptr;
    for (int i = 0; i < header->e_phnum; ++i) {
        const Elf64_Phdr& ph = phdrs[i];
        if (ph.p_type != PT_LOAD) continue;
        if (rela_vaddr >= ph.p_vaddr && rela_vaddr < ph.p_vaddr + ph.p_filesz) {
            const uptr offset_in_seg = rela_vaddr - ph.p_vaddr;
            rela = reinterpret_cast<const Elf64_Rela*>(static_cast<const u8*>(file_data) + ph.p_offset + offset_in_seg);
            break;
        }
    }

    if (!rela) return true;

    const usize count = rela_sz / rela_ent;

    for (usize i = 0; i < count; ++i) {
        const auto& [r_offset, r_info, r_addend] = rela[i];

        if (const u32 type = ELF64_R_TYPE(r_info); type == R_X86_64_RELATIVE) {
            const uptr target_vaddr = r_offset + load_bias;

            const phys_addr_t phys = realm_get_phys(realm, target_vaddr);
            if (phys_null(phys)) continue;

            auto* where = static_cast<u64*>(phys_to_virt(phys).ptr);
            *where = load_bias + r_addend;
        }
    }

    return true;
}

// Validation

bool ElfLoader::validate_magic(const Elf64_Ehdr* header) {
    return header->e_ident[0] == ELFMAG0 && header->e_ident[1] == ELFMAG1 && header->e_ident[2] == ELFMAG2 &&
           header->e_ident[3] == ELFMAG3;
}

bool ElfLoader::validate_type(const Elf64_Ehdr* header) {
    return header->e_type == ET_EXEC || header->e_type == ET_DYN;
}

bool ElfLoader::validate_architecture(const Elf64_Ehdr* header) {
    return header->e_machine == EM_X86_64;
}

ElfLoader::FileData ElfLoader::load_file_from_vfs(const char* path, Realm* realm) {
    Result<VfsNode*> file_result = VFS::open(path);
    if (file_result.is_err()) {
        return {nullptr, 0, "Failed to open file"};
    }
    VfsNode* file = file_result.unwrap();

    const usize size = file->size;
    void* data = kernel::memory::malloc(size);

    if (!data) {
        VFS::close(file);
        return {nullptr, 0, "Failed to allocate memory for file"};
    }

    kernel::security::apply_exec_credentials(realm->cred, file);

    CHRONOS_CP_PHASE("elf", "read_start");
    VFS::read(file, 0, size, data);
    CHRONOS_CP_PHASE("elf", "read_end");
    VFS::close(file);

    return {data, size, nullptr};
}

bool ElfLoader::calculate_address_range(const Elf64_Ehdr* header, const void* file_data, AddressRange& range) {
    auto* phdrs = reinterpret_cast<const Elf64_Phdr*>(static_cast<const u8*>(file_data) + header->e_phoff);

    uptr min_addr = UPTR_MAX;
    uptr max_addr = 0;
    bool found_load = false;

    for (int i = 0; i < header->e_phnum; ++i) {
        const Elf64_Phdr& ph = phdrs[i];

        if (ph.p_type != PT_LOAD) continue;

        found_load = true;

        // Start-Adresse (aligned)
        const uptr seg_start = align_down(ph.p_vaddr, ph.p_align);

        // End-Adresse (aligned)
        const uptr seg_end = align_up(ph.p_vaddr + ph.p_memsz, ph.p_align);

        if (seg_start < min_addr) min_addr = seg_start;
        if (seg_end > max_addr) max_addr = seg_end;
    }

    if (!found_load) {
        return false;
    }

    range.vaddr_min = min_addr;
    range.vaddr_max = max_addr;
    range.total_size = max_addr - min_addr;

    return true;
}

uptr ElfLoader::calculate_load_bias(const Elf64_Ehdr* header, const AddressRange& range, const uptr preferred_base) {
    if (header->e_type == ET_EXEC) {
        // ET_EXEC, no Bias
        return 0;
    }
    // ET_DYN
    return preferred_base - range.vaddr_min;
}

// =============================================================================
// SEGMENT-VERARBEITUNG
// =============================================================================

ElfLoader::SegmentMapping ElfLoader::calculate_segment_mapping(const Elf64_Phdr& ph, const uptr load_bias) {
    const uptr seg_vaddr = ph.p_vaddr + load_bias;

    const uptr page_start = align_down(seg_vaddr, PAGE_SIZE);
    const usize page_offset = seg_vaddr - page_start;

    const usize file_size = ph.p_filesz;
    const usize memory_size = ph.p_memsz;

    const usize total_needed = page_offset + memory_size;
    const usize map_size = align_up(total_needed, PAGE_SIZE);

    return {
        .page_start = page_start,
        .page_offset = page_offset,
        .map_size = map_size,
        .file_size = file_size,
        .memory_size = memory_size
    };
}

bool ElfLoader::load_segment(const Elf64_Phdr& phdr, const void* file_data, const uptr load_bias, const Realm* realm) {
    auto [page_start, page_offset, map_size, file_size, memory_size] = calculate_segment_mapping(phdr, load_bias);

    ELF_LOG(
        "[ELF] Loading segment: vaddr=0x%lx -> 0x%lx, size=0x%lx/0x%lx, flags=%c%c%c",
        phdr.p_vaddr,
        phdr.p_vaddr + load_bias,
        file_size,
        memory_size,
        (phdr.p_flags & PF_R) ? 'R' : '-',
        (phdr.p_flags & PF_W) ? 'W' : '-',
        (phdr.p_flags & PF_X) ? 'X' : '-'
    );

    const phys_addr_t phys = kernel::memory::request_pages_phys(map_size / PAGE_SIZE);
    if (phys_null(phys)) {
        ELF_LOG("[ELF] Failed to allocate physical memory for segment");
        return false;
    }

    const virt_addr_t virt = phys_to_virt(phys);
    memset(virt, 0, map_size);

    if (file_size > 0) {
        memcpy(virt_as<u8>(virt_add(virt, page_offset)), static_cast<const u8*>(file_data) + phdr.p_offset, file_size);
    }

    if (memory_size > file_size) {
        memset(virt_as<u8>(virt_add(virt, page_offset + file_size)), 0, memory_size - file_size);
    }

    u64 pt_flags = (1ULL << PtFlag::Present) | (1ULL << PtFlag::UserSuper);
    if (phdr.p_flags & PF_W) pt_flags |= (1ULL << PtFlag::ReadWrite);

    realm->address_space->page_table()->map_range(virt_from_raw(page_start), phys, map_size, pt_flags);

    return true;
}

bool ElfLoader::process_all_segments(
    const Elf64_Ehdr* header, const void* file_data, const uptr load_bias, const Realm* realm
) {
    auto* phdrs = reinterpret_cast<const Elf64_Phdr*>(static_cast<const u8*>(file_data) + header->e_phoff);

    for (int i = 0; i < header->e_phnum; ++i) {
        const Elf64_Phdr& ph = phdrs[i];

        if (ph.p_type != PT_LOAD) continue;

        if (!load_segment(ph, file_data, load_bias, realm)) {
            ELF_LOG("[ELF] Failed to load segment %d", i);
            return false;
        }
    }

    return true;
}

ElfLoader::TlsInfo ElfLoader::find_tls_segment(const Elf64_Ehdr* header, const void* file_data) {
    auto* phdrs = reinterpret_cast<const Elf64_Phdr*>(static_cast<const u8*>(file_data) + header->e_phoff);
    for (int i = 0; i < header->e_phnum; ++i) {
        if (phdrs[i].p_type == PT_TLS) {
            return {
                .file_offset = phdrs[i].p_offset,
                .file_size = phdrs[i].p_filesz,
                .mem_size = phdrs[i].p_memsz,
                .align = phdrs[i].p_align ? phdrs[i].p_align : 1,
                .present = true
            };
        }
    }
    return {0, 0, 0, 1, false};
}

uptr ElfLoader::align_down(const uptr v, const usize align) {
    return v & ~(align - 1);
}

uptr ElfLoader::align_up(const uptr v, const usize align) {
    return (v + align - 1) & ~(align - 1);
}