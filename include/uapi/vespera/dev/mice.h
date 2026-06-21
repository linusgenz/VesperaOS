// mice.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 27.05.26.
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

#ifndef VESPERAOS_UAPI_DEV_MICE_H
#define VESPERAOS_UAPI_DEV_MICE_H

#include <vespera/types.h>

typedef enum mice_event_type {
    MICE_EVENT_MOVE = 0,
    MICE_EVENT_BUTTON = 1,
    MICE_EVENT_SCROLL = 2,
} mice_event_type;

typedef enum mice_button {
    MICE_BUTTON_NONE = -1,
    MICE_BUTTON_LEFT = 0,
    MICE_BUTTON_RIGHT = 1,
    MICE_BUTTON_MIDDLE = 2,
    MICE_BUTTON_SIDE1 = 3,
    MICE_BUTTON_SIDE2 = 4,
} mice_button;

#define MICE_BTN_LEFT (1u << 0)
#define MICE_BTN_RIGHT (1u << 1)
#define MICE_BTN_MIDDLE (1u << 2)
#define MICE_BTN_SIDE1 (1u << 3)
#define MICE_BTN_SIDE2 (1u << 4)

typedef struct mice_event {
    mice_event_type type;   // Which kind of event (see MICE_EVENT_*)
    u32 buttons;            // MICE_BTN_* bitmask — full state after this event
    i32 dx;                 // Relative X delta   (MOVE only,   positive = right)
    i32 dy;                 // Relative Y delta   (MOVE only,   positive = down)
    i32 scroll;             // Scroll wheel delta (SCROLL only, positive = away from user)
    mice_button button_id;  // Which button changed (BUTTON only)
} mice_event;

#endif  // VESPERAOS_UAPI_DEV_MICE_H
