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

#include "intel_debug_identifier.h"
#include "common/intel_gem.h"
#include "iris/iris_bufmgr.h"
#include "iris/iris_batch.h"
#include "iris/iris_context.h"

#include "vespera/dev/lucifer_drm.h"

static uint32_t
lucifer_gem_create(
    struct iris_bufmgr* bufmgr,
    const struct intel_memory_class_instance** regions,
    uint16_t regions_count, uint64_t size,
    enum iris_heap heap_flags, enum bo_alloc_flags alloc_flags
) {
    const struct intel_device_info* devinfo = iris_bufmgr_get_device_info(bufmgr);

    struct lucifer_gem_create gem_create = {
        .size = align64(size, devinfo->mem_alignment),
    };

    /* Gen9.5 has no local/VRAM memory classes, so region instances collapse
     * onto a single system-memory placement bit */
    for (uint16_t i = 0; i < regions_count; i++)
        gem_create.placement |= BITFIELD_BIT(regions[i]->instance);

    if (alloc_flags & BO_ALLOC_SCANOUT)
        gem_create.flags |= LUCIFER_GEM_CREATE_FLAG_SCANOUT;

    const struct intel_device_info_pat_entry* pat_entry =
        iris_heap_to_pat_entry(devinfo, heap_flags, alloc_flags & BO_ALLOC_SCANOUT);
    switch (pat_entry->mmap) {
        case INTEL_DEVICE_INFO_MMAP_MODE_WC:
            gem_create.cpu_caching = LUCIFER_GEM_CPU_CACHING_WC;
            break;
        case INTEL_DEVICE_INFO_MMAP_MODE_WB:
        case INTEL_DEVICE_INFO_MMAP_MODE_INVALID:
        default:
            gem_create.cpu_caching = LUCIFER_GEM_CPU_CACHING_WB;
            break;
    }

    if (intel_ioctl(iris_bufmgr_get_fd(bufmgr), LUCIFER_IOCTL_GEM_CREATE, &gem_create))
        return 0;

    return gem_create.handle;
}

static uint32_t
lucifer_gem_create_userptr(struct iris_bufmgr* bufmgr, void* ptr, uint64_t size) {
    struct lucifer_gem_userptr userptr = {
        .ptr  = (uintptr_t)ptr,
        .size = size,
    };

    if (intel_ioctl(iris_bufmgr_get_fd(bufmgr), LUCIFER_IOCTL_GEM_USERPTR, &userptr))
        return 0;

    return userptr.handle;
}

static int
lucifer_gem_close(struct iris_bufmgr* bufmgr, struct iris_bo* bo) {
    if (bo->real.userptr)
        return 0;

    struct lucifer_gem_close close = {
        .handle = bo->gem_handle,
    };
    return intel_ioctl(iris_bufmgr_get_fd(bufmgr), LUCIFER_IOCTL_GEM_CLOSE, &close);
}

static bool
lucifer_bo_madvise(struct iris_bo* bo, enum iris_madvice state) {
    struct iris_bufmgr* bufmgr = bo->bufmgr;

    struct lucifer_gem_madvise madvise = {
        .handle = bo->gem_handle,
        .state  = state,
    };

    /* Same values between iris_madvice and lucifer's madvice enum */
    STATIC_ASSERT(IRIS_MADVICE_WILL_NEED == LUCIFER_MADVICE_WILL_NEED);
    STATIC_ASSERT(IRIS_MADVICE_DONT_NEED == LUCIFER_MADVICE_DONT_NEED);

    if (intel_ioctl(iris_bufmgr_get_fd(bufmgr), LUCIFER_IOCTL_GEM_MADVISE, &madvise)) {
        fprintf(stderr, "lucifer_bo_madvise: LUCIFER_IOCTL_GEM_MADVISE failed(%i)\n", -errno);
        return false;
    }

    return madvise.retained;
}

static int
lucifer_bo_set_caching(struct iris_bo* bo, bool cached) {
    UNREACHABLE("lucifer_bo_set_caching not implemented");
    return 0;
}

static void *
lucifer_gem_mmap(struct iris_bufmgr *bufmgr, struct iris_bo *bo)
{
    struct lucifer_gem_mmap_offset args = {
        .handle = bo->gem_handle,
     };

    if (intel_ioctl(iris_bufmgr_get_fd(bufmgr), LUCIFER_IOCTL_GEM_MMAP_OFFSET, &args))
        return NULL;

    void *map = mmap(NULL, bo->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                     iris_bufmgr_get_fd(bufmgr), args.offset);

    return map != MAP_FAILED ? map : NULL;
}

static enum pipe_reset_status
lucifer_batch_check_for_reset(struct iris_batch* batch) {
    UNREACHABLE("lucifer_batch_check_for_reset not implemented");
    return (enum pipe_reset_status)0;
}

static int
lucifer_batch_submit(struct iris_batch* batch) {
    struct iris_bufmgr* bufmgr = batch->screen->bufmgr;
    simple_mtx_t *bo_deps_lock = iris_bufmgr_get_bo_deps_lock(bufmgr);
    struct lucifer_sync* syncs = NULL;
    unsigned num_syncs;
    int ret;

    iris_bo_unmap(batch->bo);

    if (INTEL_DEBUG(DEBUG_BATCH) && intel_debug_batch_in_range(batch->ice->frame))
        iris_batch_decode_batch(batch);

    simple_mtx_lock(bo_deps_lock);

    /* Sets up batch->syncobjs[0] (the signaling syncobj for this
     * submission) and appends any wait syncobjs coming from other
     * batches/contexts into batch->exec_fences. Everything read below
     * (exec_fences, the signal syncobj) is populated here, so the lock
     * must stay held until after we've consumed it in the ioctl. */
    iris_batch_update_syncobjs(batch);

    num_syncs = iris_batch_num_fences(batch) + 1 /* our own signal */;
    syncs = calloc(num_syncs, sizeof(*syncs));
    if (!syncs)
        return -ENOMEM;

    unsigned i = 0;
    util_dynarray_foreach(&batch->exec_fences, struct iris_batch_fence, fence) {
        syncs[i].handle = fence->handle;
        syncs[i].flags  = (fence->flags & IRIS_BATCH_FENCE_SIGNAL) ?
            LUCIFER_SYNC_FLAG_SIGNAL : 0;
        i++;
    }

    /* The batch's own completion syncobj -- always signalled. */
    syncs[i].handle = iris_batch_get_signal_syncobj(batch)->handle;
    syncs[i].flags  = LUCIFER_SYNC_FLAG_SIGNAL;
    i++;

    assert(i == num_syncs);

    if ((INTEL_DEBUG(DEBUG_BATCH) &&
     intel_debug_batch_in_range(batch->ice->frame)) ||
    INTEL_DEBUG(DEBUG_SUBMIT)) {
        iris_dump_fence_list(batch);
        iris_dump_bo_list(batch);
    }

    struct lucifer_exec exec = {
        .vm_id      = iris_bufmgr_get_global_vm_id(bufmgr),
        .engine     = batch->lucifer.engine_class,
        .batch_addr = batch->exec_bos[0]->address,
        .batch_len  = batch->primary_batch_size,
        .syncs      = (uintptr_t)syncs,
        .num_syncs  = num_syncs,
    };

    ret = intel_ioctl(iris_bufmgr_get_fd(bufmgr), LUCIFER_IOCTL_EXEC, &exec);
    if (ret) {
        ret = -errno;
        fprintf(stderr, "lucifer_batch_submit: LUCIFER_IOCTL_EXEC failed(%i)\n", ret);
    }

    simple_mtx_unlock(bo_deps_lock);

    free(syncs);

    for (int b = 0; b < batch->exec_count; b++) {
        struct iris_bo* bo = batch->exec_bos[b];

        bo->idle = false;
        bo->index = -1;

        iris_get_backing_bo(bo)->idle = false;

        iris_bo_unreference(bo);
    }

    return ret;
}


static bool
lucifer_gem_vm_bind_op(struct iris_bo* bo, enum lucifer_vm_bind_op op, enum bo_alloc_flags iris_flags) {
    struct iris_bufmgr* bufmgr = bo->bufmgr;
    const intel_device_info* devinfo = iris_bufmgr_get_device_info(bufmgr);

    uint32_t handle = op == LUCIFER_VM_BIND_OP_UNMAP ? 0 : bo->gem_handle;
    uint64_t obj_offset = 0;
    uint64_t range;

    if (iris_bo_is_imported(bo))
        range = bo->size;
    else
        range = align64(bo->size, devinfo->mem_alignment);

    if (bo->real.userptr) {
        handle = 0;
        obj_offset = (uintptr_t)bo->real.map;
        if (op == LUCIFER_VM_BIND_OP_MAP)
            op = LUCIFER_VM_BIND_OP_MAP_USERPTR;
    }

    uint32_t flags = 0;
    if (bo->real.capture)
        flags |= LUCIFER_VM_BIND_FLAG_DUMPABLE;

    struct lucifer_vm_bind args = {
        .vm_id      = iris_bufmgr_get_global_vm_id(bufmgr),
        .op         = op,
        .handle     = handle,
        .pat_index  = iris_heap_to_pat_entry(devinfo, bo->real.heap, bo->real.scanout)->index,
        .obj_offset = obj_offset,
        .range      = range,
        .addr       = intel_48b_address(bo->address),
        .flags      = flags,
    };

    fprintf(stderr, "vm_bind: bo=%p size=%lu range=%lu addr=0x%lx\n", bo, bo->size, range, args.addr);

    const int ret = intel_ioctl(iris_bufmgr_get_fd(bufmgr), LUCIFER_IOCTL_VM_BIND, &args);
    if (ret)
        fprintf(stderr, "lucifer_gem_vm_bind_op: LUCIFER_IOCTL_VM_BIND failed(%i)\n", ret);

    return ret;
}

static bool
lucifer_gem_vm_bind(struct iris_bo* bo, enum bo_alloc_flags flags) {
    return lucifer_gem_vm_bind_op(bo, LUCIFER_VM_BIND_OP_MAP, flags) == 0;
}

static bool
lucifer_gem_vm_unbind(struct iris_bo* bo) {
    return lucifer_gem_vm_bind_op(bo, LUCIFER_VM_BIND_OP_UNMAP, 0) == 0;
}

const struct iris_kmd_backend* lucifer_get_backend(void) {
    static const struct iris_kmd_backend lucifer_kmd_backend = {
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
