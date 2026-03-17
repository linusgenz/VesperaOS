// keycode.h
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
#ifndef VESPERAOS_KEYCODE_H
#define VESPERAOS_KEYCODE_H

#include <vespera/types.h>

/**
 * @brief Hardware-independent key identifier.
 *
 * Values are based on USB HID Usage IDs (Usage Page 0x07).
 * Both the PS/2 and xHCI drivers translate their raw scancodes / HID
 * usages into this enum before publishing an InputEvent.
 *
 * Reference: USB HID Usage Tables 1.3, Section 10 "Keyboard/Keypad Page"
 * https://usb.org/sites/default/files/hut1_3_0.pdf  (page 88 ff.)
 */
enum class KeyCode : u32 {
    UNKNOWN = 0x00,

    // Printable
    A = 0x04,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,  // 0x04 – 0x1D

    N1 = 0x1E,
    N2,
    N3,
    N4,
    N5,
    N6,
    N7,
    N8,
    N9,
    N0 = 0x27,

    ENTER = 0x28,
    ESCAPE = 0x29,
    BACKSPACE = 0x2A,
    TAB = 0x2B,
    SPACE = 0x2C,

    MINUS = 0x2D,          // - _
    EQUALS = 0x2E,         // = +
    LEFT_BRACKET = 0x2F,   // [ {
    RIGHT_BRACKET = 0x30,  // ] }
    BACKSLASH = 0x31,      // \ |
    SEMICOLON = 0x33,      // ; :
    APOSTROPHE = 0x34,     // ' "
    GRAVE = 0x35,          // ` ~
    COMMA = 0x36,          // ,
    PERIOD = 0x37,         // . >
    SLASH = 0x38,          // / ?

    // Function keys
    F1 = 0x3A,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7 = 0x40,
    F8,
    F9,
    F10,
    F11,
    F12,  // 0x3A – 0x45

    // System / lock keys
    PRINT_SCREEN = 0x46,
    SCROLL_LOCK = 0x47,
    PAUSE = 0x48,

    // Navigation cluster
    INSERT = 0x49,
    HOME = 0x4A,
    PAGE_UP = 0x4B,
    DELETE = 0x4C,
    END = 0x4D,
    PAGE_DOWN = 0x4E,

    // Arrow keys
    ARROW_RIGHT = 0x4F,
    ARROW_LEFT = 0x50,
    ARROW_DOWN = 0x51,
    ARROW_UP = 0x52,

    // Numpad
    NUM_LOCK = 0x53,
    KP_SLASH = 0x54,
    KP_STAR = 0x55,
    KP_MINUS = 0x56,
    KP_PLUS = 0x57,
    KP_ENTER = 0x58,
    KP_1 = 0x59,
    KP_2,
    KP_3,
    KP_4,
    KP_5,
    KP_6 = 0x5F,
    KP_7,
    KP_8,
    KP_9,
    KP_0 = 0x62,
    KP_DOT = 0x63,

    // Modifier keys (HID 0xE0–0xE7)
    LEFT_CTRL = 0xE0,
    LEFT_SHIFT = 0xE1,
    LEFT_ALT = 0xE2,
    LEFT_SUPER = 0xE3,
    RIGHT_CTRL = 0xE4,
    RIGHT_SHIFT = 0xE5,
    RIGHT_ALT = 0xE6,
    RIGHT_SUPER = 0xE7,
};

#endif  // VESPERAOS_KEYCODE_H
