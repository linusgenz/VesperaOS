// cpustat.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 31.03.26.
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
#ifndef VESPERAOS_CPUSTAT_H
#define VESPERAOS_CPUSTAT_H

#include <vespera/devices/char_device.h>

class CpuStatDevice final : public CharDevice {
public:
    explicit CpuStatDevice();

    int   open(CharFile** out_cf) override;
    int   release(CharFile* cf)   override;
    isize read(CharFile* cf, void* buffer, usize count, usize offset) override;
    isize write(CharFile* cf, const void* buffer, usize count) override;
};


#endif  // VESPERAOS_CPUSTAT_H
