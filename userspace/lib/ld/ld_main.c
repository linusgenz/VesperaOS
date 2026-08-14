// ld_main.c
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

#include <stdint.h>
#include <stddef.h>

#include "elf.h"
#include "elf_loader.h"
#include "sys/mman.h"

extern int64_t ld_raw_open(const char* path, int64_t flags, int64_t mode);
extern int64_t ld_raw_close(int64_t fd);
extern int64_t ld_raw_read(int64_t fd, void* buf, uint64_t count);
extern int64_t ld_raw_write(int64_t fd, const void* buf, uint64_t count);
extern int64_t ld_raw_seek(int64_t fd, int64_t offset, int64_t whence);
extern void    ld_raw_exit(int64_t code) __attribute__((noreturn));
extern int64_t ld_raw_mmap(void* addr, uint64_t length, int prot, int flags, int64_t fd, int64_t offset);
extern int64_t ld_raw_mprotect(void* addr, uint64_t length, int prot);
extern int64_t ld_raw_munmap(void* addr, uint64_t length);

static uint64_t ld_strlen(const char* s);

void* memset(void* dest, int c, size_t num) {
    uint8_t* d = (uint8_t*)dest;

    for (size_t i = 0; i < num; i++) {
        d[i] = (uint8_t)c;
    }

    return dest;
}

void* memcpy(void* dest, const void* src, size_t len) {
    char* d = (char*)dest;
    const char* s = (char*)src;
    while (len--) *d++ = *s++;
    return dest;
}

void* elf_loader_sys_mmap(void* addr, uint64_t size, int prot, int fixed) {
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | (fixed ? MAP_FIXED : 0);
    int64_t result = ld_raw_mmap(addr, size, prot, flags, -1, 0);
    if (result < 0 && result > -4096) {
        return (void*)-1;
    }
    return (void*)result;
}

int elf_loader_sys_mprotect(void* addr, uint64_t size, int prot) {
    return (int)ld_raw_mprotect(addr, size, prot);
}

int elf_loader_sys_munmap(void* addr, uint64_t size) {
    return (int)ld_raw_munmap(addr, size);
}

extern void ld_selfreloc_apply(uint64_t load_bias, const Elf64_Dyn* dyn_runtime);

extern Elf64_Dyn _DYNAMIC[];

#define LD_MAX_OBJECTS 64
#define LD_MAX_PATH    256
#define LD_FILE_BUF_MAX (16 * 1024 * 1024) // 16 MiB Obergrenze je Datei

typedef struct {
    elf_loaded_object_t obj;
    char     path[LD_MAX_PATH];
    int      in_use;
} ld_slot_t;

static ld_slot_t g_slots[LD_MAX_OBJECTS];
static int       g_slot_count = 0;

static uint8_t g_file_buf[LD_FILE_BUF_MAX];

static const char* const kSearchPaths[] = {
    "/lib/",
    "/usr/lib/",
    ""
};
#define LD_SEARCH_PATHS_N (sizeof(kSearchPaths) / sizeof(kSearchPaths[0]))

static int ld_strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static uint64_t ld_strlen(const char* s) {
    uint64_t n = 0;
    while (s[n]) n++;
    return n;
}

static void ld_strcpy_bounded(char* dst, const char* src, uint64_t dst_size) {
    uint64_t i = 0;
    for (; i + 1 < dst_size && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

static void ld_path_concat(char* dst, uint64_t dst_size, const char* a, const char* b) {
    uint64_t la = ld_strlen(a);
    if (la >= dst_size) la = dst_size - 1;
    for (uint64_t i = 0; i < la; i++) dst[i] = a[i];
    ld_strcpy_bounded(dst + la, b, dst_size - la);
}


static ld_slot_t* ld_alloc_slot(void) {
    for (int i = 0; i < LD_MAX_OBJECTS; i++) {
        if (!g_slots[i].in_use) {
            g_slots[i].in_use = 1;
            return &g_slots[i];
        }
    }
    return NULL;
}

static ld_slot_t* ld_find_by_path(const char* path) {
    for (int i = 0; i < LD_MAX_OBJECTS; i++) {
        if (g_slots[i].in_use && ld_strcmp(g_slots[i].path, path) == 0) {
            return &g_slots[i];
        }
    }
    return NULL;
}

static int64_t ld_read_whole_file(const char* path) {
    int64_t fd = ld_raw_open(path, /*O_RDONLY=*/0, 0);
    if (fd < 0) return -1;

    int64_t size = ld_raw_seek(fd, 0, /*SEEK_END=*/2);
    if (size < 0 || size > LD_FILE_BUF_MAX) {
        ld_raw_close(fd);
        return -1;
    }
    ld_raw_seek(fd, 0, /*SEEK_SET=*/0);

    int64_t total = 0;
    while (total < size) {
        int64_t n = ld_raw_read(fd, g_file_buf + total, (uint64_t)(size - total));
        if (n <= 0) {
            ld_raw_close(fd);
            return -1;
        }
        total += n;
    }

    ld_raw_close(fd);
    return size;
}

// Attempts to find `name` in the standard search paths (or directly, if
// absolute). Writes the first match to `out` and returns 1,
// otherwise 0.
static int ld_resolve_path(const char* name, char* out, uint64_t out_size) {
    if (name[0] == '/') {
        ld_strcpy_bounded(out, name, out_size);
        return 1;
    }

    for (uint64_t i = 0; i < LD_SEARCH_PATHS_N; i++) {
        ld_path_concat(out, out_size, kSearchPaths[i], name);
        int64_t fd = ld_raw_open(out, /*O_RDONLY=*/0, 0);
        if (fd >= 0) {
            ld_raw_close(fd);
            return 1;
        }
    }

    ld_strcpy_bounded(out, name, out_size);
    return 0;
}

static ld_slot_t* ld_load_object(const char* name, int depth) {
    if (depth > 16) {
        ld_raw_exit(127);
    }

    char resolved[LD_MAX_PATH];
    ld_resolve_path(name, resolved, sizeof(resolved));

    ld_slot_t* existing = ld_find_by_path(resolved);
    if (existing) {
        return existing;
    }

    int64_t file_size = ld_read_whole_file(resolved);
    if (file_size < (int64_t)sizeof(Elf64_Ehdr)) {
        ld_raw_exit(126);
    }

    Elf64_Ehdr* eh = (Elf64_Ehdr*)g_file_buf;
    if (!elf_loader_validate_ehdr(eh)) {
        ld_raw_exit(126);
    }

    uint64_t vmin, vmax;
    if (!elf_loader_calc_address_range(eh, g_file_buf, &vmin, &vmax)) {
        ld_raw_exit(126);
    }

    uint64_t map_size = elf_loader_align_up(vmax - vmin, 4096ULL);

    void* probe = elf_loader_sys_mmap(NULL, map_size, PROT_NONE, 0);
    if (probe == (void*)-1) {
        ld_raw_exit(126);
    }
    elf_loader_sys_munmap(probe, map_size);

    uint64_t map_base = (uint64_t)probe;
    uint64_t load_bias = map_base - vmin;

    if (!elf_loader_map_segments(eh, g_file_buf, load_bias, map_base, map_size)) {
        ld_raw_exit(126);
    }

    ld_slot_t* slot = ld_alloc_slot();
    if (!slot) {
        ld_raw_exit(126);
    }

    ld_strcpy_bounded(slot->path, resolved, sizeof(slot->path));
    slot->obj.load_bias = load_bias;
    slot->obj.map_base = map_base;
    slot->obj.map_size = map_size;
    slot->obj.ehdr = (Elf64_Ehdr*)map_base;
    g_slot_count++;

    Elf64_Phdr* dyn_phdr = elf_loader_find_dynamic_phdr(eh);
    elf_dyn_scan_result_t scan;
    for (uint64_t i = 0; i < sizeof(scan); i++) ((uint8_t*)&scan)[i] = 0;

    if (dyn_phdr) {
        Elf64_Dyn* dyn = (Elf64_Dyn*)(dyn_phdr->p_vaddr + load_bias);
        elf_loader_scan_dynamic(&slot->obj, dyn, load_bias, &scan);
    }

    elf_loader_apply_relocations(&slot->obj, scan.rela_addr, scan.rela_size, scan.rela_ent);
    elf_loader_apply_relocations(&slot->obj, scan.jmprel_addr, scan.jmprel_size, sizeof(Elf64_Rela));

    for (uint32_t i = 0; i < scan.needed_count; i++) {
        if (!slot->obj.dynstr) continue;
        const char* dep_name = slot->obj.dynstr + scan.needed_off[i];
        ld_slot_t* dep = ld_load_object(dep_name, depth + 1);
        if (slot->obj.needed_count < ELF_LOADER_MAX_NEEDED) {
            slot->obj.needed[slot->obj.needed_count++] = &dep->obj;
        }
    }

    elf_loader_run_init_functions(&slot->obj);

    return slot;
}

typedef struct {
    uint64_t phdr;
    uint64_t phent;
    uint64_t phnum;
    uint64_t entry;
    uint64_t base;
    uint64_t pagesz;
    uint64_t main_base;
} ld_auxv_info_t;

// stack_ptr points to &argc (see ld_start.asm). Returns argc/argv/envp
// as well as the parsed auxiliary vector values.
static char** ld_parse_stack(uint64_t* stack_ptr, ld_auxv_info_t* auxv_out,
                              int64_t* argc_out, char*** envp_out) {
    int64_t argc = (int64_t)stack_ptr[0];
    char** argv = (char**)&stack_ptr[1];
    char** envp = argv + argc + 1; // +1 for the NULL after argv

    uint64_t i = 0;
    while (envp[i] != NULL) i++;
    Elf64_auxv_t* auxv = (Elf64_auxv_t*)(envp + i + 1);

    auxv_out->phdr = 0;
    auxv_out->phent = 0;
    auxv_out->phnum = 0;
    auxv_out->entry = 0;
    auxv_out->base = 0;
    auxv_out->pagesz = 4096;

    for (uint64_t j = 0; auxv[j].a_type != AT_NULL; j++) {
        switch (auxv[j].a_type) {
            case AT_PHDR:  auxv_out->phdr = auxv[j].a_un.a_val; break;
            case AT_PHENT: auxv_out->phent = auxv[j].a_un.a_val; break;
            case AT_PHNUM: auxv_out->phnum = auxv[j].a_un.a_val; break;
            case AT_ENTRY: auxv_out->entry = auxv[j].a_un.a_val; break;
            case AT_BASE:  auxv_out->base = auxv[j].a_un.a_val; break;
            case AT_PAGESZ: auxv_out->pagesz = auxv[j].a_un.a_val; break;
            case AT_VESPERA_MAIN_BASE: auxv_out->main_base = auxv[j].a_un.a_val; break;
            default: break;
        }
    }

    *argc_out = argc;
    *envp_out = envp;
    return argv;
}

__attribute__((noreturn))
static void ld_jump_to_entry(uint64_t entry, uint64_t stack_ptr,
                              int64_t argc, char** argv, char** envp) {

    __asm__ volatile(
        "mov %0, %%rsp\n"
        "xor %%rbp, %%rbp\n"   // clean stack frame, as with a standard program startup
        "mov %2, %%rdi\n"      // argc
        "mov %3, %%rsi\n"      // argv
        "mov %4, %%rdx\n"      // envp
        "jmp *%1\n"
        :
        : "r"(stack_ptr), "r"(entry), "r"(argc), "r"(argv), "r"(envp)
        : "memory", "rdi", "rsi", "rdx", "rbp", "rsp"
    );
    __builtin_unreachable();
}

void ld_start_c(uint64_t* stack_ptr) {

    ld_auxv_info_t auxv;
    int64_t argc;
    char** envp;
    char** argv = ld_parse_stack(stack_ptr, &auxv, &argc, &envp);

    uint64_t load_bias = auxv.base;

    ld_selfreloc_apply(load_bias, _DYNAMIC);

    if (!auxv.phdr || !auxv.entry) {
        ld_raw_exit(125);
    }

    Elf64_Phdr* main_phdrs = (Elf64_Phdr*)auxv.phdr;
    uint64_t main_phnum = auxv.phnum;

    uint64_t main_load_bias = 0;
    for (uint64_t i = 0; i < main_phnum; i++) {
        if (main_phdrs[i].p_type == PT_PHDR) {
            main_load_bias = auxv.phdr - main_phdrs[i].p_vaddr;
            break;
        }
    }

    elf_loaded_object_t main_obj;
    for (uint64_t i = 0; i < sizeof(main_obj); i++) ((uint8_t*)&main_obj)[i] = 0;
    main_obj.load_bias = main_load_bias;
    main_obj.map_base = 0;
    main_obj.map_size = 0;
    main_obj.ehdr = NULL;

    Elf64_Phdr* main_dyn_phdr = NULL;
    for (uint64_t i = 0; i < main_phnum; i++) {
        if (main_phdrs[i].p_type == PT_DYNAMIC) {
            main_dyn_phdr = &main_phdrs[i];
            break;
        }
    }

    elf_dyn_scan_result_t main_scan;
    for (uint64_t i = 0; i < sizeof(main_scan); i++) ((uint8_t*)&main_scan)[i] = 0;

    if (main_dyn_phdr) {
        Elf64_Dyn* main_dyn = (Elf64_Dyn*)(main_dyn_phdr->p_vaddr + main_load_bias);
        elf_loader_scan_dynamic(&main_obj, main_dyn, main_load_bias, &main_scan);
    }

    for (uint32_t i = 0; i < main_scan.needed_count; i++) {
        if (!main_obj.dynstr) continue;
        const char* dep_name = main_obj.dynstr + main_scan.needed_off[i];
        ld_slot_t* dep = ld_load_object(dep_name, 0);
        if (main_obj.needed_count < ELF_LOADER_MAX_NEEDED) {
            main_obj.needed[main_obj.needed_count++] = &dep->obj;
        }
    }

    elf_loader_apply_relocations(&main_obj, main_scan.rela_addr, main_scan.rela_size, main_scan.rela_ent);
    elf_loader_apply_relocations(&main_obj, main_scan.jmprel_addr, main_scan.jmprel_size, sizeof(Elf64_Rela));

    elf_loader_run_init_functions(&main_obj);

    ld_jump_to_entry(auxv.entry, (uint64_t)stack_ptr, argc, argv, envp);
}