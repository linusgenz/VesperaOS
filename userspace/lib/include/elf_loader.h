// elf_loader.h
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
//
// ---------------------------------------------------------------------------
// Freistehender ELF64-Loader-Kern: Mapping, DT_*-Scan, Relokation,
// Symbolsuche. Wird sowohl von dlfcn.c (Runtime dlopen()) als auch von
// ld-vespera.so (Bootstrap-Interpreter fuer PT_INTERP-Programme) genutzt.
//
// Diese Datei setzt voraus, dass folgende Symbole beim Linken verfuegbar
// sind (siehe Forward-Declarations unten):
//   - memcpy, memset                (freistehende oder vesplib-Variante)
//   - elf_loader_sys_mmap, elf_loader_sys_mprotect, elf_loader_sys_munmap
// Letztere drei sind bewusst NICHT direkt mmap/mprotect/munmap, sondern ein
// duenner Name, den der Aufrufer (dlfcn.c bzw. ld_main.c) auf den jeweils
// passenden Syscall-Wrapper mapped - so bleibt diese Datei unabhaengig
// davon, ob gerade vesplib-Wrapper oder rohe Syscalls verfuegbar sind.
// ---------------------------------------------------------------------------

#ifndef VESPERA_ELF_LOADER_H
#define VESPERA_ELF_LOADER_H

#include <stdint.h>
#include <stddef.h>

#include "elf.h"

#ifdef __cplusplus
extern "C" {
#endif


#define ELF_LOADER_MAX_NEEDED 32


extern void* memcpy(void* dst, const void* src, size_t n);
extern void* memset(void* dst, int c, size_t n);

extern void* elf_loader_sys_mmap(void* addr, uint64_t size, int prot, int fixed);
extern int   elf_loader_sys_mprotect(void* addr, uint64_t size, int prot);
extern int   elf_loader_sys_munmap(void* addr, uint64_t size);


typedef struct elf_loaded_object {
    uint64_t    load_bias;
    uint64_t    map_base;
    uint64_t    map_size;
    Elf64_Ehdr* ehdr;

    Elf64_Sym*  dynsym;
    const char* dynstr;
    uint64_t    dynsym_count;

    uint32_t*   hash;       // DT_HASH, if present (otherwise NULL)
    uint32_t*   gnu_hash;   // DT_GNU_HASH, if present (otherwise NULL)

    void*       init_fn;    // DT_INIT
    void**      init_array; // DT_INIT_ARRAY
    uint64_t    init_array_count;


    struct elf_loaded_object* needed[ELF_LOADER_MAX_NEEDED];
    uint32_t    needed_count;
} elf_loaded_object_t;

typedef struct {
    uint64_t rela_addr, rela_size, rela_ent;
    uint64_t jmprel_addr, jmprel_size;
    uint64_t needed_off[ELF_LOADER_MAX_NEEDED];
    uint32_t needed_count;
} elf_dyn_scan_result_t;

uint64_t elf_loader_align_down(uint64_t v, uint64_t align);
uint64_t elf_loader_align_up(uint64_t v, uint64_t align);


int elf_loader_validate_ehdr(const Elf64_Ehdr* eh);


int elf_loader_calc_address_range(const Elf64_Ehdr* eh, const void* file,
                                   uint64_t* min_addr, uint64_t* max_addr);


Elf64_Phdr* elf_loader_find_dynamic_phdr(const Elf64_Ehdr* eh);


Elf64_Phdr* elf_loader_find_interp_phdr(const Elf64_Ehdr* eh);

int elf_loader_map_segments(const Elf64_Ehdr* eh, const void* file,
                             uint64_t load_bias, uint64_t map_base,
                             uint64_t map_size);


void elf_loader_scan_dynamic(elf_loaded_object_t* obj, const Elf64_Dyn* dyn,
                              uint64_t load_bias, elf_dyn_scan_result_t* out);


Elf64_Sym* elf_loader_find_symbol_in_object(elf_loaded_object_t* obj, const char* name);

Elf64_Sym* elf_loader_find_symbol_recursive(elf_loaded_object_t* obj, const char* name,
                                             elf_loaded_object_t** found_in, int depth);

void elf_loader_apply_relocations(elf_loaded_object_t* obj, uint64_t rela_addr,
                                   uint64_t rela_size, uint64_t rela_ent);


void elf_loader_run_init_functions(elf_loaded_object_t* obj);

#ifdef __cplusplus
}
#endif

#endif // VESPERA_ELF_LOADER_H