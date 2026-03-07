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

#include <vespera/input/input_manager.h>
#include <vespera/mm/memory.h>

#include "xhci.h"

constexpr usize MAX_KEYS = 6;

struct HidKeymapEntry
{
    u8 usage;
    char normal;
    char shifted;
};

static constexpr HidKeymapEntry HID_KEYMAP[] = {
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
    {0x2E, '=', '+'}, {0x2F, '[', '{'}, {0x30, ']', '}'}, {0x31, '\\', '|'},
    {0x33, ';', ':'}, {0x34, '\'', '"'}, {0x35, '`', '~'}, {0x36, ',', '<'},
    {0x37, '.', '>'}, {0x38, '/', '?'}
};

static char translate_hid_usage_to_ascii(u8 usage_id, u32 modifiers)
{
    bool shift = (modifiers & (KBD_MOD_LSHIFT | KBD_MOD_RSHIFT)) != 0;

    for (const auto& entry : HID_KEYMAP)
    {
        if (entry.usage == usage_id)
        {
            return shift ? entry.shifted : entry.normal;
        }
    }

    return '?'; // Unknown
}

void XhciKeyboardDriver::on_device_init(usb::XhciDriver* hcd)
{
    memset(prev_keys_, 0, sizeof(prev_keys_));

    char name[16];
    DeviceManager::alloc_unique_device_name("usb_kbd", name, sizeof(name));
    device_ = new UsbKeyboardDevice(name, hcd->get_device());
}

void XhciKeyboardDriver::on_device_event(u8* data)
{
    const u8* current_keys = &data[2];
    u8 modifier_byte = data[0];

    process_input_report(current_keys, modifier_byte);
}

void XhciKeyboardDriver::process_input_report(
    const u8* current_keys, u8 modifier_byte
)
{
    u32 modifiers = modifier_byte;

    // --- Handle Key Presses ---
    for (usize i = 0; i < MAX_KEYS; ++i)
    {
        u8 key = current_keys[i];
        if (key == 0) continue;

        bool was_previously_pressed = false;
        for (unsigned char m_prev_key : prev_keys_)
        {
            if (m_prev_key == key)
            {
                was_previously_pressed = true;
                break;
            }
        }

        if (!was_previously_pressed)
        {
            char ascii = translate_hid_usage_to_ascii(key, modifiers);

            alignas(16) u8 ev_buffer[sizeof(kernel::input::InputEvent)];
            auto* ev = new(ev_buffer) kernel::input::InputEvent{
                .device = kernel::input::InputDeviceType::KEYBOARD,
                .keycode = key,
                .modifiers = modifiers,
                .action = kernel::input::KeyAction::PRESS,
                .ascii = ascii
            };

            kernel::input::InputManager::push_event(*ev);
        }
    }

    // --- Handle Key Releases ---
    for (unsigned char key : prev_keys_)
    {
        if (key == 0) continue;

        bool is_still_pressed = false;
        for (usize j = 0; j < MAX_KEYS; ++j)
        {
            if (current_keys[j] == key)
            {
                is_still_pressed = true;
                break;
            }
        }

        if (!is_still_pressed)
        {
            char ascii = translate_hid_usage_to_ascii(key, modifiers);

            alignas(16) u8 ev_buffer[sizeof(kernel::input::InputEvent)];
            auto* ev = new(ev_buffer) kernel::input::InputEvent{
                .device = kernel::input::InputDeviceType::KEYBOARD,
                .keycode = key,
                .modifiers = modifiers,
                .action = kernel::input::KeyAction::RELEASE,
                .ascii = ascii
            };

            kernel::input::InputManager::push_event(*ev);
        }
    }


    // Update previous keys buffer
    memcpy(prev_keys_, current_keys, MAX_KEYS);
}

void XhciKeyboardDriver::detach()
{
    delete device_;
}
