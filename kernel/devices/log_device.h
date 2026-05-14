// log_device.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 01.10.25.
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

#ifndef VESPERAOS_LOG_DEVICE_H
#define VESPERAOS_LOG_DEVICE_H

#include <vespera/devices/char_device.h>
#include <vespera/ipc/channel.h>

class LogDevice final : public CharDevice {
   private:
    Channel* global_channel_;

   public:
    explicit LogDevice(Channel* ch)
        : CharDevice(BusType::VIRTUAL)
        , global_channel_(ch) {
    }

    ~LogDevice() override {
        Channel::destroy(global_channel_);
    }

    int open(CharFile** out_cf) override;

    int release(CharFile* cf) override;

    isize read(CharFile* cf, void* buffer, usize count, usize offset) override;

    isize write(CharFile* cf, const void* buffer, usize count) override;

    int poll(CharFile* cf) override;
};

#endif  // VESPERAOS_LOG_DEVICE_H