// dlfcn.c
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

#include "dlfcn.h"

#include <fflags.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysstd.h>

#include "elf.h"
#include "elf_loader.h"

#define DL_MAX_LOADED     64
#define DL_MAX_PATH       256
#define DL_ERR_BUF_SIZE   256
#define DL_SEARCH_PATHS_N 3

#define PAGE_SIZE_ 4096ULL

static const char* const kDefaultSearchPaths[DL_SEARCH_PATHS_N] = {
    "/lib/",
    "/usr/lib/",
    ""
};

typedef struct dl_object {
    elf_loaded_object_t base;

    char     path[DL_MAX_PATH];
    uint32_t refcount;
    int      in_use; // 0 = free
} dl_object_t;

static dl_object_t g_objects[DL_MAX_LOADED];
static _Thread_local char g_dlerror_buf[DL_ERR_BUF_SIZE];
static _Thread_local int  g_dlerror_pending = 0;

static void dl_set_error(const char* fmt, const char* arg) {
    if (arg) {
        snprintf(g_dlerror_buf, sizeof(g_dlerror_buf), fmt, arg);
    } else {
        snprintf(g_dlerror_buf, sizeof(g_dlerror_buf), "%s", fmt);
    }
    g_dlerror_pending = 1;
}

char* dlerror(void) {
    if (!g_dlerror_pending) {
        return NULL;
    }
    g_dlerror_pending = 0;
    return g_dlerror_buf;
}

void* elf_loader_sys_mmap(void* addr, uint64_t size, int prot, int fixed) {
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | (fixed ? MAP_FIXED : 0);
    void* result = mmap(addr, (size_t)size, prot, flags, (uint64_t)-1, 0);
    return result == MAP_FAILED ? (void*)-1 : result;
}

int elf_loader_sys_mprotect(void* addr, uint64_t size, int prot) {
    return mprotect(addr, (size_t)size, prot);
}

int elf_loader_sys_munmap(void* addr, uint64_t size) {
    return munmap(addr, (size_t)size);
}

static dl_object_t* alloc_object_slot(void) {
    for (int i = 0; i < DL_MAX_LOADED; i++) {
        if (!g_objects[i].in_use) {
            memset(&g_objects[i], 0, sizeof(dl_object_t));
            g_objects[i].in_use = 1;
            return &g_objects[i];
        }
    }
    return NULL;
}

static dl_object_t* find_object_by_path(const char* path) {
    for (int i = 0; i < DL_MAX_LOADED; i++) {
        if (g_objects[i].in_use && strcmp(g_objects[i].path, path) == 0) {
            return &g_objects[i];
        }
    }
    return NULL;
}


static void* read_whole_file(const char* path, uint64_t* out_size) {
    int64_t hid = sys_open((uint64_t)path, O_RDONLY, 0, 0, 0, 0);
    if (hid < 0) {
        return NULL;
    }

    int64_t size = sys_seek((uint64_t)hid, 0, SEEK_END, 0, 0, 0);
    if (size < 0) {
        sys_close((uint64_t)hid, 0, 0, 0, 0, 0);
        return NULL;
    }
    sys_seek((uint64_t)hid, 0, SEEK_SET, 0, 0, 0);

    void* buf = malloc((size_t)size);
    if (!buf) {
        sys_close((uint64_t)hid, 0, 0, 0, 0, 0);
        return NULL;
    }

    uint64_t total_read = 0;
    while (total_read < (uint64_t)size) {
        int64_t n = sys_read((uint64_t)hid, (uint64_t)((uint8_t*)buf + total_read),
                              (uint64_t)size - total_read, 0, 0, 0);
        if (n <= 0) {
            free(buf);
            sys_close((uint64_t)hid, 0, 0, 0, 0, 0);
            return NULL;
        }
        total_read += (uint64_t)n;
    }

    sys_close((uint64_t)hid, 0, 0, 0, 0, 0);
    *out_size = (uint64_t)size;
    return buf;
}

static int resolve_path(const char* name, char* out, size_t out_size) {
    if (name[0] == '/') {
        snprintf(out, out_size, "%s", name);
        return 1;
    }

    for (int i = 0; i < DL_SEARCH_PATHS_N; i++) {
        snprintf(out, out_size, "%s%s", kDefaultSearchPaths[i], name);
        int64_t hid = sys_open((uint64_t)out, O_RDONLY, 0, 0, 0, 0);
        if (hid >= 0) {
            sys_close((uint64_t)hid, 0, 0, 0, 0, 0);
            return 1;
        }
    }

    snprintf(out, out_size, "%s", name);
    return 0;
}

static dl_object_t* load_object(const char* name, int depth) {
    if (depth > 16) {
        dl_set_error("dlopen: dependency chain too deep near '%s'", name);
        return NULL;
    }

    char resolved[DL_MAX_PATH];
    resolve_path(name, resolved, sizeof(resolved));

    dl_object_t* existing = find_object_by_path(resolved);
    if (existing) {
        existing->refcount++;
        return existing;
    }

    uint64_t file_size = 0;
    void* file_data = read_whole_file(resolved, &file_size);
    if (!file_data || file_size < sizeof(Elf64_Ehdr)) {
        dl_set_error("dlopen: cannot open '%s'", resolved);
        if (file_data) free(file_data);
        return NULL;
    }

    Elf64_Ehdr* eh = (Elf64_Ehdr*)file_data;
    if (!elf_loader_validate_ehdr(eh)) {
        dl_set_error("dlopen: '%s' is not a valid x86_64 ELF shared object", resolved);
        free(file_data);
        return NULL;
    }

    uint64_t vmin, vmax;
    if (!elf_loader_calc_address_range(eh, file_data, &vmin, &vmax)) {
        dl_set_error("dlopen: '%s' has no loadable segments", resolved);
        free(file_data);
        return NULL;
    }

    uint64_t map_size = elf_loader_align_up(vmax - vmin, PAGE_SIZE_);

    uint64_t load_bias = 0;
    uint64_t map_base = 0;

    if (eh->e_type == ET_DYN) {
        void* probe = mmap(NULL, (size_t)map_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, (uint64_t)-1, 0);
        if (probe == MAP_FAILED) {
            dl_set_error("dlopen: out of address space for '%s'", resolved);
            free(file_data);
            return NULL;
        }
        munmap(probe, (size_t)map_size);
        map_base = (uint64_t)probe;
        load_bias = map_base - vmin;
    } else {
        load_bias = 0;
        map_base = vmin;
    }

    if (!elf_loader_map_segments(eh, file_data, load_bias, map_base, map_size)) {
        dl_set_error("dlopen: failed to map segments for '%s'", resolved);
        free(file_data);
        return NULL;
    }

    dl_object_t* obj = alloc_object_slot();
    if (!obj) {
        dl_set_error("dlopen: too many loaded objects (limit reached)", NULL);
        munmap((void*)map_base, (size_t)map_size);
        free(file_data);
        return NULL;
    }

    snprintf(obj->path, sizeof(obj->path), "%s", resolved);
    obj->base.load_bias = load_bias;
    obj->base.map_base = map_base;
    obj->base.map_size = map_size;
    obj->base.ehdr = (Elf64_Ehdr*)map_base;
    obj->refcount = 1;

    Elf64_Phdr* dyn_phdr = elf_loader_find_dynamic_phdr(eh);
    elf_dyn_scan_result_t scan;
    memset(&scan, 0, sizeof(scan));

    if (dyn_phdr) {
        Elf64_Dyn* dyn = (Elf64_Dyn*)(dyn_phdr->p_vaddr + load_bias);
        elf_loader_scan_dynamic(&obj->base, dyn, load_bias, &scan);
    }

    free(file_data);

    elf_loader_apply_relocations(&obj->base, scan.rela_addr, scan.rela_size, scan.rela_ent);
    elf_loader_apply_relocations(&obj->base, scan.jmprel_addr, scan.jmprel_size, sizeof(Elf64_Rela));

    for (uint32_t i = 0; i < scan.needed_count; i++) {
        if (!obj->base.dynstr) continue;
        const char* dep_name = obj->base.dynstr + scan.needed_off[i];
        dl_object_t* dep = load_object(dep_name, depth + 1);
        if (!dep) {
            munmap((void*)obj->base.map_base, (size_t)obj->base.map_size);
            obj->in_use = 0;
            return NULL;
        }
        if (obj->base.needed_count < ELF_LOADER_MAX_NEEDED) {
            obj->base.needed[obj->base.needed_count++] = &dep->base;
        }
    }

    elf_loader_run_init_functions(&obj->base);

    return obj;
}

void* dlopen(const char* path, int mode) {
    if (!path) {
        dl_set_error("dlopen: path is NULL", NULL);
        return NULL;
    }

    if ((mode & (RTLD_LAZY | RTLD_NOW)) == 0) {
        dl_set_error("dlopen: mode must specify RTLD_LAZY or RTLD_NOW", NULL);
        return NULL;
    }

    // RTLD_LAZY/RTLD_NOW and RTLD_GLOBAL/RTLD_LOCAL are currently accepted only
    // as flags; all relocations are resolved eagerly upon loading, as no
    // PLT trampoline mechanism is implemented.
    // RTLD_GLOBAL does not yet affect visibility for other objects
    // (all loaded objects are currently globally searchable via their own
    // "needed" chain).
    (void)mode;

    dl_object_t* obj = load_object(path, 0);
    if (!obj) {
        return NULL;
    }

    return (void*)obj;
}

void* dlsym(void* handle, const char* name) {
    if (!handle || !name) {
        dl_set_error("dlsym: invalid arguments", NULL);
        return NULL;
    }

    dl_object_t* obj = (dl_object_t*)handle;
    if (!obj->in_use) {
        dl_set_error("dlsym: stale handle", NULL);
        return NULL;
    }

    elf_loaded_object_t* owner = NULL;
    Elf64_Sym* sym = elf_loader_find_symbol_recursive(&obj->base, name, &owner, 0);
    if (!sym) {
        dl_set_error("dlsym: symbol '%s' not found", name);
        return NULL;
    }

    return (void*)(sym->st_value + owner->load_bias);
}

int dlclose(void* handle) {
    if (!handle) {
        dl_set_error("dlclose: invalid handle", NULL);
        return -1;
    }

    dl_object_t* obj = (dl_object_t*)handle;
    if (!obj->in_use) {
        dl_set_error("dlclose: stale handle", NULL);
        return -1;
    }

    if (obj->refcount > 1) {
        obj->refcount--;
        return 0;
    }

    // Close dependencies first (decrement refcount, unmap if necessary)
    for (uint32_t i = 0; i < obj->base.needed_count; i++) {
        dlclose((void*)obj->base.needed[i]);
    }

    munmap((void*)obj->base.map_base, (size_t)obj->base.map_size);
    memset(obj, 0, sizeof(*obj));
    obj->in_use = 0;

    return 0;
}