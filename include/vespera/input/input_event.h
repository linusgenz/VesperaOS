// input_event.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 09.09.25.
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

#ifndef VESPERAOS_INPUT_EVENT_H
#define VESPERAOS_INPUT_EVENT_H

#include <vespera/types.h>

#include "keycode.h"

namespace kernel::input {

    using ModMask = u32;

    static constexpr ModMask MOD_LCTRL = (1 << 0);
    static constexpr ModMask MOD_LSHIFT = (1 << 1);
    static constexpr ModMask MOD_LALT = (1 << 2);
    static constexpr ModMask MOD_LSUPER = (1 << 3);
    static constexpr ModMask MOD_RCTRL = (1 << 4);
    static constexpr ModMask MOD_RSHIFT = (1 << 5);
    static constexpr ModMask MOD_RALT = (1 << 6);
    static constexpr ModMask MOD_RSUPER = (1 << 7);

    static constexpr ModMask MOD_SHIFT = MOD_LSHIFT | MOD_RSHIFT;
    static constexpr ModMask MOD_CTRL = MOD_LCTRL | MOD_RCTRL;
    static constexpr ModMask MOD_ALT = MOD_LALT | MOD_RALT;
    static constexpr ModMask MOD_SUPER = MOD_LSUPER | MOD_RSUPER;

    enum class InputDeviceType { KEYBOARD, MOUSE, CONTROLLER, TOUCH, UNKNOWN };

    enum class KeyAction { PRESS, RELEASE };

    enum class MouseButton { NONE = 0, LEFT = (1 << 0), RIGHT = (1 << 1), MIDDLE = (1 << 2) };
    using MouseButtonMask = u8;

    struct KeyboardEvent {
        KeyCode keycode;
        ModMask modifiers;
        KeyAction action;
        char ascii;
    };

    struct MouseEvent {
        u32 x;
        u32 y;
        i32 delta_x;
        i32 delta_y;
        i8 wheel_delta;
        MouseButtonMask buttons_pressed;
    };

    struct InputEvent {
        InputDeviceType device;

        union {
            KeyboardEvent key;
            MouseEvent mouse;
        };
    };

}  // namespace kernel::input

#endif  // VESPERAOS_INPUT_EVENT_H