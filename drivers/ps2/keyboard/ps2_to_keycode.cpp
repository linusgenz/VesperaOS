// ps2_to_keycode.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 17.03.26.
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

#include <vespera/input/keycode.h>
#include <vespera/types.h>

namespace ps2::keyboard {

    /**
     * Reference: https://wiki.osdev.org/PS/2_Keyboard#Scan_Code_Sets
     */
    KeyCode ps2_to_keycode(u8 base_scancode, bool e0) {
        if (e0) {
            switch (base_scancode) {
                case 0x1D:
                    return KeyCode::RIGHT_CTRL;
                case 0x38:
                    return KeyCode::RIGHT_ALT;
                case 0x47:
                    return KeyCode::HOME;
                case 0x48:
                    return KeyCode::ARROW_UP;
                case 0x49:
                    return KeyCode::PAGE_UP;
                case 0x4B:
                    return KeyCode::ARROW_LEFT;
                case 0x4D:
                    return KeyCode::ARROW_RIGHT;
                case 0x4F:
                    return KeyCode::END;
                case 0x50:
                    return KeyCode::ARROW_DOWN;
                case 0x51:
                    return KeyCode::PAGE_DOWN;
                case 0x52:
                    return KeyCode::INSERT;
                case 0x53:
                    return KeyCode::DELETE;
                case 0x5B:
                    return KeyCode::LEFT_SUPER;
                case 0x5C:
                    return KeyCode::RIGHT_SUPER;
                default:
                    return KeyCode::UNKNOWN;
            }
        }

        switch (base_scancode) {
            case 0x01:
                return KeyCode::ESCAPE;
            case 0x02:
                return KeyCode::N1;
            case 0x03:
                return KeyCode::N2;
            case 0x04:
                return KeyCode::N3;
            case 0x05:
                return KeyCode::N4;
            case 0x06:
                return KeyCode::N5;
            case 0x07:
                return KeyCode::N6;
            case 0x08:
                return KeyCode::N7;
            case 0x09:
                return KeyCode::N8;
            case 0x0A:
                return KeyCode::N9;
            case 0x0B:
                return KeyCode::N0;
            case 0x0E:
                return KeyCode::BACKSPACE;
            case 0x0F:
                return KeyCode::TAB;
            case 0x1C:
                return KeyCode::ENTER;
            case 0x1D:
                return KeyCode::LEFT_CTRL;
            case 0x1E:
                return KeyCode::A;
            case 0x1F:
                return KeyCode::S;
            case 0x20:
                return KeyCode::D;
            case 0x21:
                return KeyCode::F;
            case 0x22:
                return KeyCode::G;
            case 0x23:
                return KeyCode::H;
            case 0x24:
                return KeyCode::J;
            case 0x25:
                return KeyCode::K;
            case 0x26:
                return KeyCode::L;
            case 0x2A:
                return KeyCode::LEFT_SHIFT;
            case 0x36:
                return KeyCode::RIGHT_SHIFT;
            case 0x38:
                return KeyCode::LEFT_ALT;
            case 0x39:
                return KeyCode::SPACE;
            case 0x3A:
                return KeyCode::F1;  // Caps Lock — optional
            case 0x3B:
                return KeyCode::F1;
            case 0x3C:
                return KeyCode::F2;
            case 0x3D:
                return KeyCode::F3;
            case 0x3E:
                return KeyCode::F4;
            case 0x3F:
                return KeyCode::F5;
            case 0x40:
                return KeyCode::F6;
            case 0x41:
                return KeyCode::F7;
            case 0x42:
                return KeyCode::F8;
            case 0x43:
                return KeyCode::F9;
            case 0x44:
                return KeyCode::F10;
            case 0x57:
                return KeyCode::F11;
            case 0x58:
                return KeyCode::F12;
            case 0x10:
                return KeyCode::Q;
            case 0x11:
                return KeyCode::W;
            case 0x12:
                return KeyCode::E;
            case 0x13:
                return KeyCode::R;
            case 0x14:
                return KeyCode::T;
            case 0x15:
                return KeyCode::Y;
            case 0x16:
                return KeyCode::U;
            case 0x17:
                return KeyCode::I;
            case 0x18:
                return KeyCode::O;
            case 0x19:
                return KeyCode::P;
            case 0x2C:
                return KeyCode::Z;
            case 0x2D:
                return KeyCode::X;
            case 0x2E:
                return KeyCode::C;
            case 0x2F:
                return KeyCode::V;
            case 0x30:
                return KeyCode::B;
            case 0x31:
                return KeyCode::N;
            case 0x32:
                return KeyCode::M;
            default:
                return KeyCode::UNKNOWN;
        }
    }
}  // namespace ps2::keyboard