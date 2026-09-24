// intel_device_query.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 06.09.26.
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

#include "lucifer/intel_device_query.h"

#include <stdlib.h>

#include "common/intel_gem.h"
#include "vespera/dev/lucifer_drm.h"


void*
lucifer_device_query_alloc_fetch(int fd, uint32_t query_id, uint32_t* len) {
    struct lucifer_query query = {
        .query = query_id,
    };

    if (intel_ioctl(fd, LUCIFER_IOCTL_QUERY, &query))
        return NULL;

    void* data = calloc(1, query.size);
    if (!data)
        return NULL;

    query.data = (uintptr_t)data;
    if (intel_ioctl(fd, LUCIFER_IOCTL_QUERY, &query)) {
        free(data);
        return NULL;
    }

    if (len)
        *len = query.size;
    return data;
}
