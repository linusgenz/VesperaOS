// lucifer_drm.h
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

#ifndef LUCIFER_DRM_H
#define LUCIFER_DRM_H

#include <vespera/types.h>
#include <vespera/ioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LUCIFER_IOCTL_QUERY   IOWR('L', 0x01, struct lucifer_query)
#define LUCIFER_IOCTL_VERSION IOWR('L', 0x02, struct lucifer_version)

#define LUCIFER_IOCTL_VM_CREATE        IOWR('L', 0x03, struct lucifer_vm_create)
#define LUCIFER_IOCTL_VM_DESTROY       IOW ('L', 0x04, struct lucifer_vm_destroy)

#define LUCIFER_IOCTL_GEM_CREATE       IOWR('L', 0x05, struct lucifer_gem_create)
#define LUCIFER_IOCTL_GEM_CLOSE        IOW ('L', 0x06, struct lucifer_gem_close)
#define LUCIFER_IOCTL_GEM_MMAP_OFFSET  IOWR('L', 0x07, struct lucifer_gem_mmap_offset)
#define LUCIFER_IOCTL_GEM_USERPTR      IOWR('L', 0x08, struct lucifer_gem_userptr)
#define LUCIFER_IOCTL_GEM_MADVISE      IOWR('L', 0x09, struct lucifer_gem_madvise)
#define LUCIFER_IOCTL_GEM_SET_CACHING  IOW ('L', 0x0A, struct lucifer_gem_set_caching)

#define LUCIFER_IOCTL_VM_BIND          IOW ('L', 0x0B, struct lucifer_vm_bind)

#define LUCIFER_IOCTL_EXEC             IOWR('L', 0x0C, struct lucifer_exec)

#define LUCIFER_IOCTL_SYNCOBJ_CREATE   IOWR('L', 0x0D, struct lucifer_syncobj_create)
#define LUCIFER_IOCTL_SYNCOBJ_DESTROY  IOW ('L', 0x0E, struct lucifer_syncobj_destroy)
#define LUCIFER_IOCTL_SYNCOBJ_WAIT     IOWR('L', 0x0F, struct lucifer_syncobj_wait)

#define LUCIFER_IOCTL_GEM_SET_TILING   IOWR('L', 0x10, struct lucifer_gem_set_tiling)
#define LUCIFER_IOCTL_GEM_GET_TILING   IOWR('L', 0x11, struct lucifer_gem_get_tiling)


struct lucifer_version {
    int32_t version_major;
    int32_t version_minor;
    int32_t version_patchlevel;

    char name[16];   /**< Must be "lucifer" */
    char date[32];   /**< Build/release date, e.g. "20260826" */
    char desc[64];   /**< Short human-readable description */
};

enum lucifer_query_id {
    LUCIFER_QUERY_CONFIG      = 0,
    LUCIFER_QUERY_TOPOLOGY    = 1,
    LUCIFER_QUERY_MEM_REGIONS = 2,
    LUCIFER_QUERY_PCI_INFO    = 3,
    LUCIFER_QUERY_ENGINES     = 4,
};

struct lucifer_query {
    uint32_t query; /**< enum lucifer_query_id */

    /**
     * Two-phase fetch size.
     * In: buffer size in bytes when data != 0.
     * Out: required size in bytes. Set data = 0 to probe size.
     */
    uint32_t size;

    /** Userspace buffer pointer, or 0 to query required buffer size. */
    uint64_t data;
};

struct lucifer_query_config {
    uint16_t device_id;
    uint8_t revision;
    uint8_t pad0;

    uint8_t gt_level; /**< GT level (1 = GT1, 2 = GT2, 3 = GT3) */
    uint8_t pad1;

    uint64_t gtt_size;            /**< Total usable GGTT size in bytes */
    uint64_t timestamp_frequency; /**< GPU timestamp frequency in Hz */
    uint32_t mem_alignment;       /**< Minimum allocation alignment in bytes */
    uint32_t pad2;
};

struct lucifer_query_topology {
    uint32_t slice_mask;
    uint32_t subslice_mask;

    /** EU mask per subslice (Gen9.5 applies uniform mask across all subslices) */
    uint32_t eu_mask;

    uint32_t l3_banks;
};

struct lucifer_query_mem_regions {
    uint64_t total_size;
    uint64_t used;
};

/**
 * Engine class identifiers for LUCIFER_QUERY_ENGINES.
 *
 * Numeric values are a local UAPI contract (not required to match Xe's
 * DRM_XE_ENGINE_CLASS_* or i915's I915_ENGINE_CLASS_*) -- the Mesa-side
 * lucifer adapter (common/lucifer/intel_engine.c) is responsible for
 * translating these into Mesa's generic enum intel_engine_class.
 */
enum lucifer_engine_class {
    LUCIFER_ENGINE_CLASS_RENDER = 0,
    LUCIFER_ENGINE_CLASS_COPY   = 1,
};

struct lucifer_engine_class_instance {
    uint16_t engine_class;    /**< enum lucifer_engine_class */
    uint16_t engine_instance; /**< 0 on Gen9.5 -- one instance per class */
};

/**
 * Two-phase fetch like every other LUCIFER_QUERY_*: probe with data == 0 to
 * get the required size (num_engines * sizeof(engines[0]) + header), then
 * fetch with a buffer of that size. num_engines is set by the kernel on
 * both the probe and the fetch call.
 */
struct lucifer_query_engines {
    uint32_t num_engines;
    uint32_t pad0;
    struct lucifer_engine_class_instance engines[];
};

/**
 * PCI bus location + identity, for drmGetDevice2()/drmDevice enumeration
 * in libdrm. Mirrors drmPciBusInfo + drmPciDeviceInfo 1:1 so the userspace
 * side is a straight field copy with no translation.
 *
 * name is the DeviceManager-assigned node name (e.g. "dri/card0"), without
 * a leading "/dev/" — userspace prepends that itself when building
 * drmDevice::nodes[].
 */
struct lucifer_query_pci_info {
    /* Bus location (== struct pci_id) */
    uint16_t domain;
    uint8_t bus;
    uint8_t dev;
    uint8_t func;
    uint8_t pad0[3];

    /* Device identity (== relevant INTEL_IGP_PCI_CONFIG fields) */
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_device_id;
    uint8_t revision;
    uint8_t pad1[3];

    /* DeviceManager node name, e.g. "dri/card0" */
    char name[16];
};

/**
 * One PPGTT ("vm") per realm.
 *
 * A realm calls LUCIFER_IOCTL_VM_CREATE once (typically right after opening
 * the device node) and gets back a vm_id identifying its private GPU
 * address space. GEM objects are not tied to a vm_id at creation time --
 * mirrors Xe -- they only become resident in a given PPGTT once bound via
 * LUCIFER_IOCTL_VM_BIND. All subsequent VM_BIND/VM_UNBIND/EXEC calls
 * reference that vm_id.
 */
enum lucifer_vm_create_flags {
    LUCIFER_VM_CREATE_FLAG_NONE = 0,
};

struct lucifer_vm_create {
    uint32_t flags; /**< enum lucifer_vm_create_flags */
    uint32_t vm_id; /**< out */
};

struct lucifer_vm_destroy {
    uint32_t vm_id;
    uint32_t pad0;
};

enum lucifer_gem_create_flags {
    LUCIFER_GEM_CREATE_FLAG_SCANOUT        = 1u << 0,
    LUCIFER_GEM_CREATE_FLAG_NEEDS_VISIBLE  = 1u << 1,
    LUCIFER_GEM_CREATE_FLAG_NO_COMPRESSION = 1u << 2,
};

enum lucifer_gem_cpu_caching {
    LUCIFER_GEM_CPU_CACHING_WB = 0,
    LUCIFER_GEM_CPU_CACHING_WC = 1,
};

struct lucifer_gem_create {
    uint64_t size; /**< In: requested size, rounded up to mem_alignment */

    uint32_t placement; /**< Bitmask of memory region instances */
    uint32_t flags;      /**< enum lucifer_gem_create_flags */

    uint32_t cpu_caching; /**< enum lucifer_gem_cpu_caching */
    uint32_t handle;       /**< out */
};

struct lucifer_gem_close {
    uint32_t handle;
    uint32_t pad0;
};

/**
 * Returns a fake mmap offset the caller then passes to VesperaOS's mmap()
 * syscall against the device fd (mirrors DRM_IOCTL_*_GEM_MMAP_OFFSET).
 */
struct lucifer_gem_mmap_offset {
    uint32_t handle; /**< in */
    uint32_t pad0;
    uint64_t offset; /**< out */
};

/**
 * Wraps an existing userspace range in a GEM handle, analogous to
 * xe_gem_create_userptr(): no backing kernel allocation happens here, the
 * range is only pinned/bound on LUCIFER_VM_BIND_OP_MAP_USERPTR.
 */
struct lucifer_gem_userptr {
    uint64_t ptr;  /**< in, user VA, page-aligned */
    uint64_t size; /**< in */

    uint32_t handle; /**< out */
    uint32_t pad0;
};

enum lucifer_madvice {
    LUCIFER_MADVICE_WILL_NEED = 0,
    LUCIFER_MADVICE_DONT_NEED = 1,
};

struct lucifer_gem_madvise {
    uint32_t handle; /**< in */
    uint32_t state;   /**< in, enum lucifer_madvice */

    uint32_t retained; /**< out, nonzero if still resident */
    uint32_t pad0;
};

struct lucifer_gem_set_caching {
    uint32_t handle;
    uint32_t cached; /**< 0 or 1 */
};

/**
 * Binds or unbinds a GEM object (or a userptr range) into the PPGTT
 * identified by vm_id at a caller-chosen GPU virtual address. Backs
 * iris_kmd_backend::gem_vm_bind / gem_vm_unbind. Synchronous: completes
 * (or fails) before the ioctl returns, no fence/sync objects involved.
 */
enum lucifer_vm_bind_op {
    LUCIFER_VM_BIND_OP_MAP         = 0,
    LUCIFER_VM_BIND_OP_UNMAP       = 1,
    LUCIFER_VM_BIND_OP_MAP_USERPTR = 2,
};

enum lucifer_vm_bind_flags {
    LUCIFER_VM_BIND_FLAG_DUMPABLE = 1u << 0,
};

struct lucifer_vm_bind {
    uint32_t vm_id;  /**< in */
    uint32_t op;      /**< in, enum lucifer_vm_bind_op */

    uint32_t handle;    /**< in, GEM handle (ignored for UNMAP) */
    uint32_t pat_index; /**< in */

    uint64_t obj_offset; /**< in, offset into the object (or user VA for MAP_USERPTR) */
    uint64_t range;       /**< in, size in bytes */
    uint64_t addr;         /**< in, GPU virtual address (48b) */

    uint32_t flags; /**< in, enum lucifer_vm_bind_flags */
    uint32_t pad0;
};

/**
 * One synchronization operation attached to an EXEC. Binary syncobjs only
 * (no timeline points) -- sufficient for bring-up where a submission either
 * waits on another submission's completion syncobj, or signals its own.
 *
 * WAIT entries are waited on by the kernel before the batch is handed to
 * the hardware. SIGNAL entries have their syncobj's fence replaced with a
 * fence for this submission's completion (i.e. armed at EXEC time, signaled
 * once the engine's completed-seqno counter reaches this submission's
 * seqno). handle == 0 is invalid -- omit the entry instead of zero-filling.
 */
enum lucifer_sync_flags {
    LUCIFER_SYNC_FLAG_SIGNAL = 1u << 0, /**< signal, rather than wait on, this handle */
};

struct lucifer_sync {
    uint32_t handle; /**< in, DRM syncobj handle */
    uint32_t flags;   /**< in, enum lucifer_sync_flags */
};

/**
 * Minimal batch submission: one batch buffer's GPU address against a
 * given vm_id / engine, plus an arbitrary set of syncobjs to wait on
 * beforehand and/or signal on completion.
 *
 * Every EXEC is assigned a monotonically increasing per-engine sequence
 * number (out_seqno) at submission time, before the batch is handed to the
 * hardware. This seqno is the fence value for this submission: it becomes
 * signaled once the engine's completed-seqno counter reaches or passes it
 * (see LUCIFER_IOCTL_SYNCOBJ_WAIT). Any handle in syncs[] with
 * LUCIFER_SYNC_FLAG_SIGNAL set is armed with this same fence, rather than
 * userspace tracking out_seqno itself -- mirroring how iris/Mesa expects
 * opaque syncobj handles to wait on.
 */
struct lucifer_exec {
    uint32_t vm_id;  /**< in */
    uint32_t engine;  /**< in, enum lucifer_engine_class */

    uint64_t batch_addr; /**< in, GPU virtual address of the batch to run */
    uint64_t batch_len;   /**< in, length in bytes */

    uint64_t syncs;        /**< in, pointer to array of struct lucifer_sync */
    uint32_t num_syncs;     /**< in */
    uint32_t pad0;

    uint64_t out_seqno; /**< out, fence value for this submission */
};

enum lucifer_tiling_mode {
    LUCIFER_TILING_NONE = 0,
    LUCIFER_TILING_X    = 1,
    LUCIFER_TILING_Y    = 2,
};

struct lucifer_gem_set_tiling {
    uint32_t handle;       /**< in */
    uint32_t tiling_mode;  /**< in, enum lucifer_tiling_mode */
    uint32_t stride;        /**< in, bytes -- row pitch for the tiled layout */
    uint32_t pad0;
};

struct lucifer_gem_get_tiling {
    uint32_t handle;       /**< in */
    uint32_t tiling_mode;  /**< out, enum lucifer_tiling_mode */
    uint32_t stride;        /**< out */
    uint32_t pad0;
};

#ifdef __cplusplus
}
#endif

#endif /* LUCIFER_DRM_H */
