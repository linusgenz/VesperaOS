// tty_device.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 25.09.25.
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

#ifndef VESPERAOS_TTY_DEVICE_H
#define VESPERAOS_TTY_DEVICE_H

#include <vespera/terminal.h>
#include <vespera/tty/tty.h>

#include "../../filesystem/devfs/devfs.h"
#include <vespera/devices/char_device.h>
#include "vespera/devices/device_manager.h"

class TtyDevice final : public CharDevice {
   public:
    kernel::tty::TTY* tty;

    explicit TtyDevice(const char* name, kernel::tty::TTY* tty_ptr)
        : CharDevice( BusType::Tty)
        , tty(tty_ptr)
        , kd_(DeviceManager::register_char_device(
              this, name, DeviceClass::Pseudo, BusType::Tty, ControllerType::None, nullptr
          )) {
        DevFs::register_device(kd_);
    }

    ~TtyDevice() override {
        DevFs::unregister_device(kd_);
        DeviceManager::unregister_device(kd_);
    }

    int open(CharFile** out_cf) override {
        *out_cf = new CharFile(this);
        return 0;
    }

    int release(CharFile* cf) override {
        delete cf;
        return 0;
    }

    isize read(CharFile*, void* buffer, usize count, usize) override {
        if (count == 0 || !buffer) return -EINVAL;
        return kernel::tty::tty_read(tty, static_cast<char*>(buffer), count);
    }

    isize write(CharFile*, const void* buffer, usize count) override {
        if (count == 0 || !buffer) return -EINVAL;
        const auto buf = static_cast<const char*>(buffer);
        for (usize i = 0; i < count; i++) {
            kernel::tty::tty_process_output(tty, buf[i]);
        }
        tty->term->flush();
        return static_cast<int>(count);
    }

   private:
    KernelDevice* kd_{};
};

#endif  // VESPERAOS_TTY_DEVICE_H
