// cpustat.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 31.03.26.
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
#ifndef VESPERAOS_CPU_USAGE_H
#define VESPERAOS_CPU_USAGE_H

#include <vespera/types.h>

#define MAX_CPU_CORES 64

/**
 * @brief Per-CPU usage statistics.
 *
 * Represents usage and cycle counters for a single CPU core.
 * All values are snapshots taken at the time of collection.
 */
typedef struct cpu_usage_stat {
    u32 cpu_id;         ///< Logical CPU ID
    u32 usage_percent;  ///< CPU usage in percent (0–100)
    u64 total_cycles;   ///< Total number of CPU cycles
    u64 idle_cycles;    ///< Number of idle CPU cycles
} cpu_usage_stat_t;

/**
 * @brief Aggregate CPU usage information for all cores.
 *
 * Contains an array of per-core usage statistics along with the
 * total number of CPUs present in the system.
 */
typedef struct cpu_usage_info {
    u32 cpu_count;                         ///< Number of active CPUs
    cpu_usage_stat_t cpus[MAX_CPU_CORES];  ///< Per-CPU statistics array
} cpu_usage_info_t;

#endif // VESPERAOS_CPU_USAGE_H