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

enum lucifer_query_id {
    LUCIFER_QUERY_CONFIG      = 0,
    LUCIFER_QUERY_TOPOLOGY    = 1,
    LUCIFER_QUERY_MEM_REGIONS = 2,
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

#ifdef __cplusplus
}
#endif

#endif /* LUCIFER_DRM_H */
