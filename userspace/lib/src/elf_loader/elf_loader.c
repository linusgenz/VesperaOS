// elf_loader.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 13.08.26.
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

#include "elf_loader.h"

#include "sys/mman.h"

#define PAGE_SIZE_ 4096ULL

uint64_t elf_loader_align_down(uint64_t v, uint64_t align) {
    return v & ~(align - 1);
}

uint64_t elf_loader_align_up(uint64_t v, uint64_t align) {
    return (v + align - 1) & ~(align - 1);
}

int elf_loader_validate_ehdr(const Elf64_Ehdr* eh) {
    if (eh->e_ident[EI_MAG0] != ELFMAG0 || eh->e_ident[EI_MAG1] != ELFMAG1 ||
        eh->e_ident[EI_MAG2] != ELFMAG2 || eh->e_ident[EI_MAG3] != ELFMAG3) {
        return 0;
    }
    if (eh->e_ident[EI_CLASS] != ELFCLASS64) return 0;
    if (eh->e_machine != EM_X86_64) return 0;
    if (eh->e_type != ET_DYN && eh->e_type != ET_EXEC) return 0;
    return 1;
}

int elf_loader_calc_address_range(const Elf64_Ehdr* eh, const void* file,
                                   uint64_t* min_addr, uint64_t* max_addr) {
    const Elf64_Phdr* phdrs = (const Elf64_Phdr*)((const uint8_t*)file + eh->e_phoff);
    uint64_t lo = UINT64_MAX, hi = 0;
    int found = 0;

    for (int i = 0; i < eh->e_phnum; i++) {
        const Elf64_Phdr* ph = &phdrs[i];
        if (ph->p_type != PT_LOAD) continue;
        found = 1;
        uint64_t seg_start = elf_loader_align_down(ph->p_vaddr, ph->p_align ? ph->p_align : PAGE_SIZE_);
        uint64_t seg_end = elf_loader_align_up(ph->p_vaddr + ph->p_memsz, ph->p_align ? ph->p_align : PAGE_SIZE_);
        if (seg_start < lo) lo = seg_start;
        if (seg_end > hi) hi = seg_end;
    }

    if (!found) return 0;
    *min_addr = lo;
    *max_addr = hi;
    return 1;
}

Elf64_Phdr* elf_loader_find_dynamic_phdr(const Elf64_Ehdr* eh) {
    Elf64_Phdr* phdrs = (Elf64_Phdr*)((uint8_t*)eh + eh->e_phoff);
    for (int i = 0; i < eh->e_phnum; i++) {
        if (phdrs[i].p_type == PT_DYNAMIC) return &phdrs[i];
    }
    return NULL;
}

Elf64_Phdr* elf_loader_find_interp_phdr(const Elf64_Ehdr* eh) {
    Elf64_Phdr* phdrs = (Elf64_Phdr*)((uint8_t*)eh + eh->e_phoff);
    for (int i = 0; i < eh->e_phnum; i++) {
        if (phdrs[i].p_type == PT_INTERP) return &phdrs[i];
    }
    return NULL;
}


int elf_loader_map_segments(const Elf64_Ehdr* eh, const void* file,
                             uint64_t load_bias, uint64_t map_base,
                             uint64_t map_size) {

    void* reserved = elf_loader_sys_mmap((void*)map_base, map_size,
                                          PROT_READ | PROT_WRITE,
                                          /*fixed=*/map_base ? 1 : 0);
    if (reserved == (void*)-1) {
        return 0;
    }

    const Elf64_Phdr* phdrs = (const Elf64_Phdr*)((const uint8_t*)file + eh->e_phoff);

    for (int i = 0; i < eh->e_phnum; i++) {
        const Elf64_Phdr* ph = &phdrs[i];
        if (ph->p_type != PT_LOAD) continue;

        uint8_t* dest = (uint8_t*)(ph->p_vaddr + load_bias);

        if (ph->p_filesz > 0) {
            memcpy(dest, (const uint8_t*)file + ph->p_offset, ph->p_filesz);
        }
        if (ph->p_memsz > ph->p_filesz) {
            memset(dest + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
        }
    }


    for (int i = 0; i < eh->e_phnum; i++) {
        const Elf64_Phdr* ph = &phdrs[i];
        if (ph->p_type != PT_LOAD) continue;

        uint64_t seg_vaddr = ph->p_vaddr + load_bias;
        uint64_t page_start = elf_loader_align_down(seg_vaddr, PAGE_SIZE_);
        uint64_t page_end = elf_loader_align_up(seg_vaddr + ph->p_memsz, PAGE_SIZE_);

        int prot = PROT_NONE;
        if (ph->p_flags & PF_R) prot |= PROT_READ;
        if (ph->p_flags & PF_W) prot |= PROT_WRITE;
        if (ph->p_flags & PF_X) prot |= PROT_EXEC;

        elf_loader_sys_mprotect((void*)page_start, page_end - page_start, prot);
    }

    return 1;
}

void elf_loader_scan_dynamic(elf_loaded_object_t* obj, const Elf64_Dyn* dyn,
                              uint64_t load_bias, elf_dyn_scan_result_t* out) {
    out->rela_addr = 0;
    out->rela_size = 0;
    out->rela_ent = sizeof(Elf64_Rela);
    out->jmprel_addr = 0;
    out->jmprel_size = 0;
    out->needed_count = 0;

    for (int i = 0; dyn[i].d_tag != DT_NULL; i++) {
        switch (dyn[i].d_tag) {
            case DT_SYMTAB:
                obj->dynsym = (Elf64_Sym*)(dyn[i].d_un.d_ptr + load_bias);
                break;
            case DT_STRTAB:
                obj->dynstr = (const char*)(dyn[i].d_un.d_ptr + load_bias);
                break;
            case DT_HASH:
                obj->hash = (uint32_t*)(dyn[i].d_un.d_ptr + load_bias);
                break;
            case DT_GNU_HASH:
                obj->gnu_hash = (uint32_t*)(dyn[i].d_un.d_ptr + load_bias);
                break;
            case DT_RELA:
                out->rela_addr = dyn[i].d_un.d_ptr + load_bias;
                break;
            case DT_RELASZ:
                out->rela_size = dyn[i].d_un.d_val;
                break;
            case DT_RELAENT:
                out->rela_ent = dyn[i].d_un.d_val;
                break;
            case DT_JMPREL:
                out->jmprel_addr = dyn[i].d_un.d_ptr + load_bias;
                break;
            case DT_PLTRELSZ:
                out->jmprel_size = dyn[i].d_un.d_val;
                break;
            case DT_INIT:
                obj->init_fn = (void*)(dyn[i].d_un.d_ptr + load_bias);
                break;
            case DT_INIT_ARRAY:
                obj->init_array = (void**)(dyn[i].d_un.d_ptr + load_bias);
                break;
            case DT_INIT_ARRAYSZ:
                obj->init_array_count = dyn[i].d_un.d_val / sizeof(void*);
                break;
            case DT_NEEDED:
                if (out->needed_count < ELF_LOADER_MAX_NEEDED) {
                    out->needed_off[out->needed_count++] = dyn[i].d_un.d_val;
                }
                break;
            default:
                break;
        }
    }

    if (!obj->hash && obj->gnu_hash) {
        uint32_t nbuckets  = obj->gnu_hash[0];
        uint32_t symoffset = obj->gnu_hash[1];
        uint32_t bloom_sz  = obj->gnu_hash[2];

        uint64_t* bloom = (uint64_t*)(obj->gnu_hash + 4);
        uint32_t* buckets = (uint32_t*)(bloom + bloom_sz);
        uint32_t* chains  = buckets + nbuckets;

        uint32_t max_sym = 0;
        for (uint32_t i = 0; i < nbuckets; i++) {
            if (buckets[i] > max_sym) {
                max_sym = buckets[i];
            }
        }

        if (max_sym >= symoffset) {
            uint32_t chain_idx = max_sym - symoffset;
            while ((chains[chain_idx] & 1) == 0) {
                chain_idx++;
            }
            obj->dynsym_count = symoffset + chain_idx + 1;
        } else {
            obj->dynsym_count = symoffset;
        }
    }
}

static int elf_loader_strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

Elf64_Sym* elf_loader_find_symbol_in_object(elf_loaded_object_t* obj, const char* name) {
    if (!obj->dynsym || !obj->dynstr) return NULL;

    if (obj->dynsym_count > 0) {
        for (uint64_t i = 0; i < obj->dynsym_count; i++) {
            Elf64_Sym* sym = &obj->dynsym[i];
            if (sym->st_name == 0) continue;
            if (sym->st_shndx == SHN_UNDEF) continue;
            const char* sym_name = obj->dynstr + sym->st_name;
            if (elf_loader_strcmp(sym_name, name) == 0) {
                return sym;
            }
        }
    }
    return NULL;
}

Elf64_Sym* elf_loader_find_symbol_recursive(elf_loaded_object_t* obj, const char* name,
                                             elf_loaded_object_t** found_in, int depth) {
    if (depth > 16 || !obj) return NULL;

    Elf64_Sym* sym = elf_loader_find_symbol_in_object(obj, name);
    if (sym) {
        *found_in = obj;
        return sym;
    }

    for (uint32_t i = 0; i < obj->needed_count; i++) {
        sym = elf_loader_find_symbol_recursive(obj->needed[i], name, found_in, depth + 1);
        if (sym) return sym;
    }

    return NULL;
}

void elf_loader_apply_relocations(elf_loaded_object_t* obj, uint64_t rela_addr,
                                   uint64_t rela_size, uint64_t rela_ent) {
    if (!rela_addr || !rela_size) return;

    uint64_t count = rela_size / rela_ent;
    Elf64_Rela* relocs = (Elf64_Rela*)rela_addr;

    for (uint64_t i = 0; i < count; i++) {
        Elf64_Rela* r = &relocs[i];
        uint32_t type = (uint32_t)ELF64_R_TYPE(r->r_info);
        uint64_t sym_idx = ELF64_R_SYM(r->r_info);
        uint64_t* target = (uint64_t*)(r->r_offset + obj->load_bias);

        switch (type) {
            case R_X86_64_RELATIVE:
                *target = obj->load_bias + (uint64_t)r->r_addend;
                break;

            case R_X86_64_GLOB_DAT:
            case R_X86_64_JUMP_SLOT:
            case R_X86_64_64: {
                if (!obj->dynsym || !obj->dynstr || sym_idx == 0) {
                    *target = 0;
                    break;
                }
                Elf64_Sym* sym = &obj->dynsym[sym_idx];
                const char* sym_name = obj->dynstr + sym->st_name;

                uint64_t resolved = 0;
                if (sym->st_shndx != SHN_UNDEF) {
                    // lokal definiertes Symbol
                    resolved = sym->st_value + obj->load_bias;
                } else {
                    // In Abhaengigkeiten suchen
                    elf_loaded_object_t* owner = NULL;
                    Elf64_Sym* found = elf_loader_find_symbol_recursive(obj, sym_name, &owner, 0);
                    if (found) {
                        resolved = found->st_value + owner->load_bias;
                    }
                }

                if (type == R_X86_64_64) {
                    *target = resolved + (uint64_t)r->r_addend;
                } else {
                    *target = resolved;
                }
                break;
            }

            default:
                break;
        }
    }
}

void elf_loader_run_init_functions(elf_loaded_object_t* obj) {
    if (obj->init_fn) {
        ((void (*)(void))obj->init_fn)();
    }
    for (uint64_t i = 0; i < obj->init_array_count; i++) {
        void* fn = obj->init_array[i];
        if (fn) {
            ((void (*)(void))fn)();
        }
    }
}