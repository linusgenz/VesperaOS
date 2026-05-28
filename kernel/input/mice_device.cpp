// mice_device.cpp
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

#include <vespera/input/mice_device.h>
#include <filesystem/devfs.h>
#include <vespera/devices/device_manager.h>

namespace kernel::input {

    MiceDevice* MiceDevice::s_instance_ = nullptr;

    MiceDevice::MiceDevice() : CharDevice(BusType::VIRTUAL) {
        lock_.init();
        s_instance_ = this;

        devnode = DeviceManager::register_device(
            DeviceDescriptor{}
                .set_name("mice")
                .set_type(DeviceType::Char)
                .set_class(DeviceClass::Input)
                .with_char(this)
                .set_bus(BusType::VIRTUAL)
        );
        DevFs::register_device(devnode);
    }

    MiceDevice::~MiceDevice() {
        DevFs::unregister_device(devnode);
        DeviceManager::unregister_device(devnode);
        if (s_instance_ == this) s_instance_ = nullptr;
    }

    int MiceDevice::open(CharFile**) { return 0; }
    int MiceDevice::release(CharFile*) { return 0; }
    isize MiceDevice::write(CharFile*, const void*, usize) { return -1; } // Read-Only

    void MiceDevice::push_to_queue(MiceDevice* dev, const mice_event& ev) {
        usize next = (dev->head_ + 1) % MiceDevice::BUFFER_SIZE;
        if (next != dev->tail_) {
            dev->buffer_[dev->head_] = ev;
            dev->head_ = next;
        }
    }

    void MiceDevice::share_mouse_event(const InputEvent& ev) {
        if (!s_instance_ || ev.device != InputDeviceType::MOUSE) return;

        SpinlockGuardIrq g(s_instance_->lock_);

        u32 current_buttons = 0;
        if (ev.mouse.buttons_pressed & 0x01) current_buttons |= MICE_BTN_LEFT;
        if (ev.mouse.buttons_pressed & 0x02) current_buttons |= MICE_BTN_RIGHT;
        if (ev.mouse.buttons_pressed & 0x04) current_buttons |= MICE_BTN_MIDDLE;

        static u32 last_buttons = 0;

        // MOVE
        if (ev.mouse.delta_x != 0 || ev.mouse.delta_y != 0) {
            mice_event move_ev;
            move_ev.type = MICE_EVENT_MOVE;
            move_ev.buttons = current_buttons;
            move_ev.dx = ev.mouse.delta_x;
            move_ev.dy = ev.mouse.delta_y;
            move_ev.scroll = 0;
            move_ev.button_id = MICE_BUTTON_NONE;

            push_to_queue(s_instance_, move_ev);
        }

        // BUTTON
        if (current_buttons != last_buttons) {
            u32 changed = current_buttons ^ last_buttons;

            // We check which bits have changed and send an event for each button.
            for (int i = 0; i < 5; i++) {
                u32 mask = (1u << i);
                if (changed & mask) {
                    mice_event btn_ev;
                    btn_ev.type = MICE_EVENT_BUTTON;
                    btn_ev.buttons = current_buttons;
                    btn_ev.dx = 0;
                    btn_ev.dy = 0;
                    btn_ev.scroll = 0;
                    btn_ev.button_id = static_cast<mice_button>(i); // MICE_BUTTON_LEFT = 0, MICE_BUTTON_RIGHT = 1, etc.

                    push_to_queue(s_instance_, btn_ev);
                }
            }
            last_buttons = current_buttons;
        }

        // SCROLL
        if (ev.mouse.wheel_delta != 0) {
            mice_event scroll_ev;
            scroll_ev.type = MICE_EVENT_SCROLL;
            scroll_ev.buttons = current_buttons;
            scroll_ev.dx = 0;
            scroll_ev.dy = 0;
            scroll_ev.scroll = ev.mouse.wheel_delta;
            scroll_ev.button_id = MICE_BUTTON_NONE;

            push_to_queue(s_instance_, scroll_ev);
        }
    }

    isize MiceDevice::read(CharFile*, void* buf, usize count, usize) {
        if (!buf || count < sizeof(mice_event)) return -EINVAL;

        mice_event* user_buf = static_cast<mice_event*>(buf);
        usize events_to_read = count / sizeof(mice_event);
        usize events_read = 0;

        SpinlockGuardIrq g(lock_);

        while (events_read < events_to_read) {
            if (head_ == tail_) {
                break;
            }

            user_buf[events_read] = buffer_[tail_];
            tail_ = (tail_ + 1) % BUFFER_SIZE;
            events_read++;
        }

        if (events_read == 0) {
            return 0;
        }

        return static_cast<isize>(events_read * sizeof(mice_event));
    }

} // namespace kernel::input