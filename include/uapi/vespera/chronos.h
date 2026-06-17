// chronos.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.06.26.
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
#ifndef VESPERAOS_UAPI_CHRONOS_H
#define VESPERAOS_UAPI_CHRONOS_H

#include <vespera/types.h>

#define CHRONOS_PHASE_MAX   24u
#define CHRONOS_LABEL_MAX   48u
#define CHRONOS_MAX_EVENTS 2048u

#define CHRONOS_SOURCE_KERNEL  ((u8)0)
#define CHRONOS_SOURCE_USER    ((u8)1)

typedef struct chronos_event {
    char phase[CHRONOS_PHASE_MAX]; /**< Subsystem / phase name, NUL-terminated */
    char label[CHRONOS_LABEL_MAX]; /**< Checkpoint label,       NUL-terminated */
    u64 ts_ns; /**< Nanoseconds since system boot           */
    u64 delta_ns; /**< Delta to previous event of same emitter */
    u32 cpu_id; /**< CPU that recorded this event            */
    u32 realm_id; /**< 0 = kernel, >0 = userspace realm ID     */
    u8 source; /**< CHRONOS_SOURCE_KERNEL / _USER           */
    u8 _pad[7];
} chronos_event_t;

typedef struct chronos_user_checkpoint {
    char phase[CHRONOS_PHASE_MAX];
    char label[CHRONOS_LABEL_MAX];
} chronos_user_checkpoint_t;

#ifdef __cplusplus
namespace chronos {
    static constexpr usize PHASE_MAX = CHRONOS_PHASE_MAX;
    static constexpr usize LABEL_MAX = CHRONOS_LABEL_MAX;
    static constexpr usize MAX_EVENTS = CHRONOS_MAX_EVENTS;

    using Event = chronos_event_t;
    using UserCheckpoint = chronos_user_checkpoint_t;

    enum class EventSource : u8 {
        Kernel = CHRONOS_SOURCE_KERNEL,
        User = CHRONOS_SOURCE_USER,
    };
} // namespace chronos
#endif /* __cplusplus */

#endif //VESPERAOS_UAPI_CHRONOS_H
