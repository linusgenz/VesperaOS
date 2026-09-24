// intel_engine.h
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

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "common/intel_engine.h"

#ifdef __cplusplus
extern "C" {

#endif

struct intel_query_engine_info*
lucifer_engine_get_info(int fd);

uint16_t intel_engine_class_to_lucifer(enum intel_engine_class intel);

bool
lucifer_engines_is_guc_semaphore_functional(int fd, const struct intel_device_info* info);

#ifdef __cplusplus
}
#endif
