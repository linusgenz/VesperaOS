// urandom.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 26.09.25.
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

#ifndef VESPERAOS_URANDOM_H
#define VESPERAOS_URANDOM_H

#include <vespera/devices/char_device.h>

class URandomDevice final : public CharDevice {
   public:
    explicit URandomDevice(uint64_t seed = 881723468263953272ull);

    int open(CharFile** out_cf) override;
    int release(CharFile*) override;
    ssize_t read(CharFile*, void* buffer, size_t count, size_t offset) override;
    ssize_t write(CharFile*, const void* buffer, size_t count) override;

   private:
    void refill();
    uint8_t next();
    uint64_t state_;
    uint8_t dev_buffer_[8]{};
    size_t buffer_index_ = 8;
};

#endif  // VESPERAOS_URANDOM_H