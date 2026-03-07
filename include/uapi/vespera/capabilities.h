/**
 * @file capabilities.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 09.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef VESPERAOS_CAPABILITIES_H
#define VESPERAOS_CAPABILITIES_H
#include <stdint.h>

/**
 * @brief Type representing a set of capabilities.
 *
 * Capabilities define allowed operations or privileges for a Realm and its Units.
 * Each bit in the 64-bit value represents a specific capability.
 */
typedef uint64_t capability_set;

#define CAP_NONE          0x0000000000000000ULL ///< No capabilities
#define CAP_READ          0x0000000000000001ULL ///< Read access
#define CAP_WRITE         0x0000000000000002ULL ///< Write access
#define CAP_RW            (CAP_READ | CAP_WRITE) ///< Read and write access
#define CAP_EXECUTE       0x0000000000000004ULL ///< Execute permission
#define CAP_NETWORK_BIND  0x0000000000000020ULL ///< Ability to bind network sockets
#define CAP_UNIT_SPAWN    0x0000000000000100ULL ///< Ability to spawn Units
#define CAP_DEVICE_ACCESS 0x0000000000000200ULL ///< Access to devices
#define CAP_ALL           0xFFFFFFFFFFFFFFFFULL ///< All capabilities

#endif //VESPERAOS_CAPABILITIES_H