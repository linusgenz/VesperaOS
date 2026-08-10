// gfx_pipeline_stats_regs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 09.08.26.
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
#ifndef VESPERAOS_GFX_PIPELINE_STATS_REGS_H
#define VESPERAOS_GFX_PIPELINE_STATS_REGS_H

#include <vespera/types.h>

constexpr u32 IA_VERTICES_COUNT_OFFSET = 0x00310;
constexpr u32 IA_PRIMITIVES_COUNT_OFFSET = 0x00318;
constexpr u32 VS_INVOCATION_COUNT_OFFSET = 0x00320;
constexpr u32 GS_INVOCATION_COUNT_OFFSET = 0x00328;
constexpr u32 GS_PRIMITIVES_COUNT_OFFSET = 0x00330;
constexpr u32 CL_INVOCATION_COUNT_OFFSET = 0x00338;
constexpr u32 CL_PRIMITIVES_COUNT_OFFSET = 0x00340;
constexpr u32 PS_INVOCATION_COUNT_OFFSET = 0x00348;
constexpr u32 PS_DEPTH_COUNT_OFFSET = 0x00350;
constexpr u32 HS_INVOCATION_COUNT_OFFSET = 0x00300;
constexpr u32 DS_INVOCATION_COUNT_OFFSET = 0x00308;

#endif //VESPERAOS_GFX_PIPELINE_STATS_REGS_H
