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

#ifdef __cplusplus
}
#endif

#endif /* LUCIFER_DRM_H */
