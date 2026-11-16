// drm.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 10.09.26.
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
#ifndef _VESPERA_DRM_H_
#define _VESPERA_DRM_H_

#include <vespera/types.h>
#include <vespera/ioctl.h>
#include "../fcntl.h"

struct drm_syncobj_create {
    u32 handle;
#define DRM_SYNCOBJ_CREATE_SIGNALED (1 << 0)
    u32 flags;
};

struct drm_syncobj_destroy {
    u32 handle;
    u32 pad;
};

#define DRM_SYNCOBJ_FD_TO_HANDLE_FLAGS_IMPORT_SYNC_FILE (1 << 0)
#define DRM_SYNCOBJ_HANDLE_TO_FD_FLAGS_EXPORT_SYNC_FILE (1 << 0)
struct drm_syncobj_handle {
    u32 handle;
    u32 flags;

    i32 fd;
    u32 pad;
};

struct drm_syncobj_transfer {
    u32 src_handle;
    u32 dst_handle;
    u64 src_point;
    u64 dst_point;
    u32 flags;
    u32 pad;
};

#define DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL (1 << 0)
#define DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT (1 << 1)
#define DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE (1 << 2) /* wait for time point to become available */
#define DRM_SYNCOBJ_WAIT_FLAGS_WAIT_DEADLINE (1 << 3) /* set fence deadline to deadline_nsec */
struct drm_syncobj_wait {
    u64 handles;
    /* absolute timeout */
    i64 timeout_nsec;
    u32 count_handles;
    u32 flags;
    u32 first_signaled; /* only valid when not waiting all */
    u32 pad;
    /**
     * @deadline_nsec - fence deadline hint
     *
     * Deadline hint, in absolute CLOCK_MONOTONIC, to set on backing
     * fence(s) if the DRM_SYNCOBJ_WAIT_FLAGS_WAIT_DEADLINE flag is
     * set.
     */
    u64 deadline_nsec;
};

struct drm_syncobj_timeline_wait {
    u64 handles;
    /* wait on specific timeline point for every handles*/
    u64 points;
    /* absolute timeout */
    i64 timeout_nsec;
    u32 count_handles;
    u32 flags;
    u32 first_signaled; /* only valid when not waiting all */
    u32 pad;
    /**
     * @deadline_nsec - fence deadline hint
     *
     * Deadline hint, in absolute CLOCK_MONOTONIC, to set on backing
     * fence(s) if the DRM_SYNCOBJ_WAIT_FLAGS_WAIT_DEADLINE flag is
     * set.
     */
    u64 deadline_nsec;
};

/**
 * struct drm_syncobj_eventfd
 * @handle: syncobj handle.
 * @flags: Zero to wait for the point to be signalled, or
 *         &DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE to wait for a fence to be
 *         available for the point.
 * @point: syncobj timeline point (set to zero for binary syncobjs).
 * @fd: Existing eventfd to sent events to.
 * @pad: Must be zero.
 *
 * Register an eventfd to be signalled by a syncobj. The eventfd counter will
 * be incremented by one.
 */
struct drm_syncobj_eventfd {
    u32 handle;
    u32 flags;
    u64 point;
    i32 fd;
    u32 pad;
};


struct drm_syncobj_array {
    u64 handles;
    u32 count_handles;
    u32 pad;
};

#define DRM_SYNCOBJ_QUERY_FLAGS_LAST_SUBMITTED (1 << 0) /* last available point on timeline syncobj */
struct drm_syncobj_timeline_array {
    u64 handles;
    u64 points;
    u32 count_handles;
    u32 flags;
};

#define DRM_RDWR O_RDWR
#define DRM_CLOEXEC O_CLOEXEC
struct drm_prime_handle {
    u32 handle;

    /** Flags.. only applicable for handle->fd */
    u32 flags;

    /** Returned dmabuf file descriptor */
    u64 fd;
};

#define DRM_IOCTL_BASE          'd'
#define DRM_IO(nr)          _IO(DRM_IOCTL_BASE,nr)
#define DRM_IOR(nr,type)        _IOR(DRM_IOCTL_BASE,nr,type)
#define DRM_IOW(nr,type)        _IOW(DRM_IOCTL_BASE,nr,type)
#define DRM_IOWR(nr,type)       _IOWR(DRM_IOCTL_BASE,nr,type)

#define DRM_IOCTL_SYNCOBJ_CREATE    DRM_IOWR(0xBF, struct drm_syncobj_create)
#define DRM_IOCTL_SYNCOBJ_DESTROY    DRM_IOWR(0xC0, struct drm_syncobj_destroy)
#define DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD    DRM_IOWR(0xC1, struct drm_syncobj_handle)
#define DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE    DRM_IOWR(0xC2, struct drm_syncobj_handle)
#define DRM_IOCTL_SYNCOBJ_WAIT        DRM_IOWR(0xC3, struct drm_syncobj_wait)
#define DRM_IOCTL_SYNCOBJ_RESET        DRM_IOWR(0xC4, struct drm_syncobj_array)
#define DRM_IOCTL_SYNCOBJ_SIGNAL    DRM_IOWR(0xC5, struct drm_syncobj_array)
#define DRM_IOCTL_PRIME_HANDLE_TO_FD    DRM_IOWR(0x2d, struct drm_prime_handle)

#endif //_VESPERA_DRM_H_
