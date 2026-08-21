// intel_device_info.h
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

#pragma once

#include <stdint.h>
#include <stdbool.h>

struct intel_device_info;

bool
intel_device_info_lucifer_get_info_from_fd(int fd,
                                        struct intel_device_info *devinfo);
bool
intel_device_info_lucifer_query_regions(struct intel_device_info *devinfo,
                                     int fd, bool update);
bool
intel_device_info_lucifer_update_from_masks(struct intel_device_info *devinfo,
                                         uint32_t slice_mask,
                                         uint32_t subslice_mask,
                                         uint32_t n_eus);

void *
intel_device_info_lucifer_query_hwconfig(int fd, int32_t *len);
