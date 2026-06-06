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

#include <filesystem/devfs.h>
#include <uapi/vespera/dev/ioctl_tty.h>
#include <uapi/vespera/poll.h>
#include <vespera/devices/char_device.h>
#include <vespera/jobctl/jobctl.h>
#include <vespera/scheduling.h>
#include <vespera/terminal.h>
#include <vespera/tty/tty.h>

#include "vespera/log.h"
#include "vespera/devices/device_manager.h"

class TtyDevice final : public CharDevice {
   public:
    kernel::tty::TTY* tty;

    explicit TtyDevice(const char* name, kernel::tty::TTY* tty_ptr)
        : CharDevice(BusType::Tty)
        , tty(tty_ptr) {
        kd_ = DeviceManager::register_device(
            DeviceDescriptor{}
                .set_name(name)
                .set_type(DeviceType::Char)
                .set_class(DeviceClass::Pseudo)
                .with_char(this)
                .set_controller(ControllerType::None)
                .set_bus(BusType::Tty)
        );
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

    int ioctl(CharFile* cf, u32 cmd, void* arg) override {
        if (!tty) return -ENOTTY;
        if (!arg && cmd != IOCTL_TTY_GET_MODE && cmd != IOCTL_TTY_GET_SIZE && cmd != TIOCSCTTY) return -EINVAL;

        switch (cmd) {
        case TIOCSCTTY:
                return kernel::jobctl::assign_controlling_tty(kernel::scheduling::get_current_realm_id(), this).to_errno();
            case IOCTL_TTY_GET_MODE: {
                auto* m = static_cast<tty_mode_t*>(arg);
                m->mode = tty->canonical ? TTY_MODE_CANONICAL : TTY_MODE_RAW;
                m->echo = tty->echo;
                return 0;
            }
            case IOCTL_TTY_SET_MODE: {
                const auto* m = static_cast<const tty_mode_t*>(arg);
                tty->canonical = (m->mode == TTY_MODE_CANONICAL);
                tty->echo = (m->echo != 0);
                tty->canon_len = 0;
                tty->line_ready = false;
                tty->raw_len = 0;
                return 0;
            }
            case IOCTL_TTY_GET_SIZE: {
                auto* s = static_cast<tty_size_t*>(arg);
                s->rows = static_cast<unsigned short>(tty->term->visible_rows());
                s->cols = static_cast<unsigned short>(tty->term->visible_cols());
                return 0;
            }
            default:
                return -ENOTTY;
        }
    };

    isize read(CharFile*, void* buffer, usize count, usize) override {
        if (count == 0 || !buffer) return -EINVAL;
        if (!tty) return 0;
        return kernel::tty::tty_read(tty, static_cast<char*>(buffer), count);
    }

    isize write(CharFile*, const void* buffer, usize count) override {
        if (count == 0 || !buffer) return -EINVAL;
        if (!tty) return count;
        const auto buf = static_cast<const char*>(buffer);
        for (usize i = 0; i < count; i++) {
            kernel::tty::tty_process_output(tty, buf[i]);
        }
        tty->term->flush();
        return static_cast<int>(count);
    }

    int poll(CharFile*) override {
        if (!tty) return -ENODEV;
        int mask = POLLOUT;

        if (tty->canonical && tty->line_ready) mask |= POLLIN;
        if (!tty->canonical && tty->raw_len > 0) mask |= POLLIN;

        return mask;
    }

    bool is_tty() const override {
        return true;
    }
    TtyDevice* as_tty() override {
        return this;
    }

   private:
    KernelDevice* kd_{};
};

#endif  // VESPERAOS_TTY_DEVICE_H
