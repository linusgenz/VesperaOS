// xhci_keyboard_driver.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 01.09.25.
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

#include "xhci_keyboard_driver.h"
#include <cstdint>
#include <cstddef>
#include <log.h>
#include <memory.h>

constexpr size_t MAX_KEYS = 6;

struct hid_keymap_entry {
    uint8_t usage;
    char normal;
    char shifted;
};

static constexpr hid_keymap_entry HID_KEYMAP[] = {
    {0x04, 'a', 'A'}, {0x05, 'b', 'B'}, {0x06, 'c', 'C'}, {0x07, 'd', 'D'},
    {0x08, 'e', 'E'}, {0x09, 'f', 'F'}, {0x0A, 'g', 'G'}, {0x0B, 'h', 'H'},
    {0x0C, 'i', 'I'}, {0x0D, 'j', 'J'}, {0x0E, 'k', 'K'}, {0x0F, 'l', 'L'},
    {0x10, 'm', 'M'}, {0x11, 'n', 'N'}, {0x12, 'o', 'O'}, {0x13, 'p', 'P'},
    {0x14, 'q', 'Q'}, {0x15, 'r', 'R'}, {0x16, 's', 'S'}, {0x17, 't', 'T'},
    {0x18, 'u', 'U'}, {0x19, 'v', 'V'}, {0x1A, 'w', 'W'}, {0x1B, 'x', 'X'},
    {0x1C, 'y', 'Y'}, {0x1D, 'z', 'Z'},

    {0x1E, '1', '!'}, {0x1F, '2', '@'}, {0x20, '3', '#'}, {0x21, '4', '$'},
    {0x22, '5', '%'}, {0x23, '6', '^'}, {0x24, '7', '&'}, {0x25, '8', '*'},
    {0x26, '9', '('}, {0x27, '0', ')'},

    {0x28, '\n', '\n'}, {0x2A, '\b', '\b'}, {0x2C, ' ', ' '}, {0x2D, '-', '_'},
    {0x2E, '=', '+'}, {0x2F, '[', '{'},  {0x30, ']', '}'}, {0x31, '\\', '|'},
    {0x33, ';', ':'}, {0x34, '\'', '"'}, {0x35, '`', '~'}, {0x36, ',', '<'},
    {0x37, '.', '>'}, {0x38, '/', '?'}
};

static char translate_hid_usage_to_ascii(uint8_t usage_id, uint32_t modifiers) {
    bool shift = (modifiers & (KBD_MOD_LSHIFT | KBD_MOD_RSHIFT)) != 0;

    for (const auto& entry : HID_KEYMAP) {
        if (entry.usage == usage_id) {
            return shift ? entry.shifted : entry.normal;
        }
    }

    return '?'; // Unknown
}

void xhciKeyboardDriver::on_device_init() {
    memset(m_prev_keys, 0, sizeof(m_prev_keys));
}

void xhciKeyboardDriver::on_device_event(uint8_t* data) {
    const uint8_t* current_keys = &data[2];
    uint8_t modifier_byte = data[0];

    process_input_report(current_keys, modifier_byte);
}

void xhciKeyboardDriver::process_input_report(
    const uint8_t* current_keys, uint8_t modifier_byte
) {
    uint32_t modifiers = static_cast<uint32_t>(modifier_byte);

    // --- Handle Key Presses ---
    for (size_t i = 0; i < MAX_KEYS; ++i) {
        uint8_t key = current_keys[i];
        if (key == 0) continue;

        bool was_previously_pressed = false;
        for (size_t j = 0; j < MAX_KEYS; ++j) {
            if (m_prev_keys[j] == key) {
                was_previously_pressed = true;
                break;
            }
        }

        if (!was_previously_pressed) {
            char sdata1 = translate_hid_usage_to_ascii(key, modifiers);
            Log::Print("%c",  sdata1);
       //     emit_key_event(key, input::KBD_EVT_KEY_PRESSED, modifiers);
        }
    }

    // --- Handle Key Releases ---
    for (size_t i = 0; i < MAX_KEYS; ++i) {
        uint8_t key = m_prev_keys[i];
        if (key == 0) continue;

        bool is_still_pressed = false;
        for (size_t j = 0; j < MAX_KEYS; ++j) {
            if (current_keys[j] == key) {
                is_still_pressed = true;
                break;
            }
        }

        if (!is_still_pressed) {
         //   emit_key_event(key, input::KBD_EVT_KEY_RELEASED, modifiers);
        }
    }



    // Update previous keys buffer
    memcpy(m_prev_keys, current_keys, MAX_KEYS);
}