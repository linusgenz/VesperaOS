// thermal.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.04.26.
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

#ifndef VESPERAOS_DEVICES_MISC_THERMAL_H
#define VESPERAOS_DEVICES_MISC_THERMAL_H

#include <vespera/devices/char_device.h>
#include <vespera/types.h>

/**
 * @brief Char device that exposes CPU and ACPI thermal zone temperatures.
 */
class ThermalDevice final : public CharDevice {
   public:
    ThermalDevice();
    ~ThermalDevice() override = default;

    ThermalDevice(const ThermalDevice&) = delete;
    ThermalDevice& operator=(const ThermalDevice&) = delete;

    int open(CharFile** out_file) override;
    int release(CharFile* file) override;
    isize read(CharFile* file, void* buffer, usize count, usize offset) override;
    isize write(CharFile* file, const void* buffer, usize count) override;
};

#endif  // VESPERAOS_DEVICES_MISC_THERMAL_H