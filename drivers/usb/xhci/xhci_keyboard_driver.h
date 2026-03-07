// xhci_keyboard_driver.h
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

#ifndef VESPERAOS_XHCI_KEYBOARD_DRIVER_H
#define VESPERAOS_XHCI_KEYBOARD_DRIVER_H

#include "xhci_hid_driver.h"
#include <vespera/types.h>

#include "xhci_keyboard_device.h"
//#include <drivers/usb/hid/hid_report_parser.h>

enum KbdModMask : u32
{
    KBD_MOD_LCTRL = 1 << 0,
    KBD_MOD_LSHIFT = 1 << 1,
    KBD_MOD_LALT = 1 << 2,
    KBD_MOD_LGUI = 1 << 3,
    KBD_MOD_RCTRL = 1 << 4,
    KBD_MOD_RSHIFT = 1 << 5,
    KBD_MOD_RALT = 1 << 6,
    KBD_MOD_RGUI = 1 << 7,
};

class XhciKeyboardDriver final : public XhciHidDriver
{
public:
    XhciKeyboardDriver() = default;
    ~XhciKeyboardDriver() override = default;

    void on_device_init(usb::XhciDriver* hcd) override;
    void on_device_event(u8* data) override;

    void detach() override;

private:
    struct InputDataLayout
    {
        u16 buttons_offset;
        u16 buttons_size;
        u16 x_axis_offset;
        u16 x_axis_size;
        u16 y_axis_offset;
        u16 y_axis_size;
    } input_layout_;

    /*void initialize_input_field(
        hid::hid_report_layout& layout,
        u16 usage_page, u16 usage,
        u16& offset, u16& size,
        const char* field_name
    );*/

    void process_input_report(const u8* current_keys, u8 modifier_byte);
    //  void emit_key_event(u8 key, input::input_event_type type, u32 modifiers);

    u8 prev_keys_[6]{};

    UsbKeyboardDevice* device_;
};

#endif //VESPERAOS_XHCI_KEYBOARD_DRIVER_H
