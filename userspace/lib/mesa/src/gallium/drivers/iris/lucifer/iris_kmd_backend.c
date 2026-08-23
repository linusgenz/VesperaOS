// iris_kmd_backend.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 17.08.26.
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

#include "iris/iris_kmd_backend.h"

#include <sys/mman.h>

#include "iris/iris_bufmgr.h"
#include "iris/iris_batch.h"
#include "iris/iris_context.h"

static uint32_t
lucifer_gem_create(
    struct iris_bufmgr* bufmgr,
    const struct intel_memory_class_instance** regions,
    uint16_t regions_count, uint64_t size,
    enum iris_heap heap_flags, enum bo_alloc_flags alloc_flags
) {
    return 0;
}

static uint32_t
lucifer_gem_create_userptr(struct iris_bufmgr* bufmgr, void* ptr, uint64_t size) {
    return 0;
}

static int
lucifer_gem_close(struct iris_bufmgr* bufmgr, struct iris_bo* bo) {
    return 0;
}

static bool
lucifer_bo_madvise(struct iris_bo* bo, enum iris_madvice state) {
    return false;
}

static int
lucifer_bo_set_caching(struct iris_bo* bo, bool cached) {
    return 0;
}

static void*
lucifer_gem_mmap(struct iris_bufmgr* bufmgr, struct iris_bo* bo) {
    return NULL;
}

static enum pipe_reset_status
lucifer_batch_check_for_reset(struct iris_batch* batch) {
    return (enum pipe_reset_status)0;
}

static int
lucifer_batch_submit(struct iris_batch* batch) {
    return 0;
}

static bool
lucifer_gem_vm_bind(struct iris_bo* bo, enum bo_alloc_flags flags) {
    return false;
}

static bool
lucifer_gem_vm_unbind(struct iris_bo* bo) {
    return false;
}

const struct iris_kmd_backend* lucifer_get_backend(void) {
    const struct iris_kmd_backend lucifer_kmd_backend = {
        .gem_create            = lucifer_gem_create,
        .gem_create_userptr    = lucifer_gem_create_userptr,
        .gem_close             = lucifer_gem_close,
        .bo_madvise            = lucifer_bo_madvise,
        .bo_set_caching        = lucifer_bo_set_caching,
        .gem_mmap              = lucifer_gem_mmap,
        .batch_check_for_reset = lucifer_batch_check_for_reset,
        .batch_submit          = lucifer_batch_submit,
        .gem_vm_bind           = lucifer_gem_vm_bind,
        .gem_vm_unbind         = lucifer_gem_vm_unbind,
    };
    return &lucifer_kmd_backend;
}
