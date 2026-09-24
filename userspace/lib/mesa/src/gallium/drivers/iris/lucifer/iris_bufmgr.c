// iris_bufmgr.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 07.09.26.
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


#include "lucifer/iris_bufmgr.h"

#include "common/intel_gem.h"
#include "intel/dev/intel_debug.h"
#include "iris/iris_bufmgr.h"

#include "isl.h"
#include "vespera/dev/lucifer_drm.h"

#define FILE_DEBUG_FLAG DEBUG_BUFMGR

bool
iris_lucifer_init_global_vm(struct iris_bufmgr *bufmgr, uint32_t *vm_id)
{
    struct lucifer_vm_create create = {
        .flags = LUCIFER_VM_CREATE_FLAG_NONE,
     };
    if (intel_ioctl(iris_bufmgr_get_fd(bufmgr), LUCIFER_IOCTL_VM_CREATE, &create))
        return false;
    *vm_id = create.vm_id;
    return true;
}

bool
iris_lucifer_destroy_global_vm(struct iris_bufmgr *bufmgr)
{
    struct lucifer_vm_destroy destroy = {
        .vm_id = iris_bufmgr_get_global_vm_id(bufmgr),
     };
    return intel_ioctl(iris_bufmgr_get_fd(bufmgr), LUCIFER_IOCTL_VM_DESTROY,
                       &destroy) == 0;
}

static inline uint32_t
isl_tiling_to_lucifer_tiling(enum isl_tiling tiling)
{
    switch (tiling) {
        case ISL_TILING_LINEAR:
            return LUCIFER_TILING_NONE;
        case ISL_TILING_X:
            return LUCIFER_TILING_X;
        case ISL_TILING_Y0:
            return LUCIFER_TILING_Y;
        default:
            UNREACHABLE("unsupported isl_tiling for lucifer uapi");
    }
}

int iris_lucifer_bo_get_tiling(struct iris_bo *bo, uint32_t *tiling)
{
    struct iris_bufmgr *bufmgr = bo->bufmgr;
    struct lucifer_gem_get_tiling ti = { .handle = bo->gem_handle };
    int ret = intel_ioctl(iris_bufmgr_get_fd(bufmgr), LUCIFER_IOCTL_GEM_GET_TILING, &ti);

    if (ret) {
        DBG("lucifer_gem_get_tiling failed for BO %u: %s\n",
            bo->gem_handle, strerror(errno));
    }

    *tiling = ti.tiling_mode;

    return ret;
}

int iris_lucifer_bo_set_tiling(struct iris_bo *bo, const struct isl_surf *surf)
{
    struct iris_bufmgr *bufmgr = bo->bufmgr;
    uint32_t tiling_mode = isl_tiling_to_lucifer_tiling(surf->tiling);
    int ret;

    struct lucifer_gem_set_tiling set_tiling = {
        .handle = bo->gem_handle,
        .tiling_mode = tiling_mode,
        .stride = surf->row_pitch_B,
     };

    ret = intel_ioctl(iris_bufmgr_get_fd(bufmgr), LUCIFER_IOCTL_GEM_SET_TILING, &set_tiling);
    if (ret) {
        DBG("lucifer_gem_set_tiling failed for BO %u: %s\n",
            bo->gem_handle, strerror(errno));
    }

    return ret;
}