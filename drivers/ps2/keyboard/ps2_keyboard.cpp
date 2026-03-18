// ps2_keyboard.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 30.07.25.
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

#include "ps2_keyboard.h"

#include "qwerty.h"
#include "vespera/input/input_manager.h"

namespace ps2::keyboard {

    struct KeyboardState {
        kernel::input::ModMask modifiers = 0;
    };
    static KeyboardState g_keyboard;
    static bool e0_prefix = false;

    void init() {
        g_keyboard.modifiers = 0;
    }

    void handle_scancode(const u8 scancode) {
        using namespace qwerty;

        if (scancode == 0xE0) {
            e0_prefix = true;
            return;
        }

        const bool pressed = (scancode & 0x80) == 0;
        const u8 base_scancode = scancode & 0x7F;
        const bool e0_was_set = e0_prefix;

        kernel::input::ModMask mod = 0;
        if (e0_prefix) {
            switch (base_scancode) {
                case RIGHT_CTRL:
                    mod = kernel::input::MOD_RCTRL;
                    break;
                case RIGHT_ALT:
                    mod = kernel::input::MOD_RALT;
                    break;
                case LEFT_SUPER:
                    mod = kernel::input::MOD_LSUPER;
                    break;
                case RIGHT_SUPER:
                    mod = kernel::input::MOD_RSUPER;
                    break;
                default:
                    break;
            }
            e0_prefix = false;
        } else {
            switch (base_scancode) {
                case LEFT_SHIFT:
                    mod = kernel::input::MOD_LSHIFT;
                    break;
                case RIGHT_SHIFT:
                    mod = kernel::input::MOD_RSHIFT;
                    break;
                case LEFT_CTRL:
                    mod = kernel::input::MOD_LCTRL;
                    break;
                case LEFT_ALT:
                    mod = kernel::input::MOD_LALT;
                    break;
                default:
                    break;
            }
        }

        if (mod) {
            if (pressed)
                g_keyboard.modifiers |= mod;
            else
                g_keyboard.modifiers &= ~mod;

            kernel::input::InputEvent ev{
                .device = kernel::input::InputDeviceType::KEYBOARD,
                .keycode = ps2_to_keycode(base_scancode, e0_was_set),
                .modifiers = g_keyboard.modifiers,
                .action = pressed ? kernel::input::KeyAction::PRESS : kernel::input::KeyAction::RELEASE,
                .ascii = 0
            };
            kernel::input::InputManager::push_event(ev);
            return;
        }

        char ascii = 0;

        const bool ctrl = (g_keyboard.modifiers & kernel::input::MOD_CTRL) != 0;
        const bool alt = (g_keyboard.modifiers & kernel::input::MOD_ALT) != 0;
        const bool super = (g_keyboard.modifiers & kernel::input::MOD_SUPER) != 0;
        const bool shift = (g_keyboard.modifiers & kernel::input::MOD_SHIFT) != 0;

        if (base_scancode == BACKSPACE)
            ascii = '\b';
        else if (base_scancode == ENTER)
            ascii = '\n';
        else if (base_scancode == SPACEBAR)
            ascii = ctrl ? 0x00 : ' ';
        else if (!alt && !super) {
            if (ctrl) {
                char base = translate(base_scancode, shift);
                if (base >= 'a' && base <= 'z')
                    ascii = base - 'a' + 1;
                else if (base >= 'A' && base <= 'Z')
                    ascii = base - 'A' + 1;
                else if (base == '[')
                    ascii = 0x1B;  // Ctrl+[ → ESC
                else if (base == '\\')
                    ascii = 0x1C;  // Ctrl+\ → FS
                else if (base == ']')
                    ascii = 0x1D;  // Ctrl+] → GS
            } else {
                ascii = translate(base_scancode, shift);
            }
        }

        // E0-prefixed keys have no ascii, keycode-only event
        if (e0_was_set) {
            kernel::input::InputEvent ev{
                .device = kernel::input::InputDeviceType::KEYBOARD,
                .keycode = ps2_to_keycode(base_scancode, true),
                .modifiers = g_keyboard.modifiers,
                .action = pressed ? kernel::input::KeyAction::PRESS : kernel::input::KeyAction::RELEASE,
                .ascii = 0
            };
            kernel::input::InputManager::push_event(ev);
            return;
        }

        if (ascii != 0) {
            kernel::input::InputEvent ev{
                .device = kernel::input::InputDeviceType::KEYBOARD,
                .keycode = ps2_to_keycode(base_scancode, e0_was_set),
                .modifiers = g_keyboard.modifiers,
                .action = pressed ? kernel::input::KeyAction::PRESS : kernel::input::KeyAction::RELEASE,
                .ascii = ascii
            };
            kernel::input::InputManager::push_event(ev);
        }
    }

}  // namespace ps2::keyboard
