// zero.h
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

#ifndef VESPERAOS_ZERO_H
#define VESPERAOS_ZERO_H

#include <vespera/devices/char_device.h>

class ZeroDevice final : public CharDevice {
   public:
    explicit ZeroDevice();

    int open(CharFile** out_cf) override;
    int release(CharFile* cf) override;
    ssize_t read(CharFile* cf, void* buffer, size_t count, size_t offset) override;
    ssize_t write(CharFile* cf, const void* buffer, size_t count) override;
};

#endif  // VESPERAOS_ZERO_H