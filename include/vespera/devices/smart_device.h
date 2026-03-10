// smart_device.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 09.03.26.
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
#ifndef VESPERAOS_SMART_DEVICE_H
#define VESPERAOS_SMART_DEVICE_H

#include <uapi/vespera/dev/ioctl_smart.h>

class ISmartDevice {
public:
    virtual bool smart_read_data(u8* out_buf)          = 0;
    virtual bool smart_get_common(SmartCommon* out)    = 0;
    virtual bool smart_get_nvme(SmartNvme* out)        { return false; }
    virtual bool smart_get_ata(SmartAta* out)          { return false; }
    virtual ~ISmartDevice() = default;
};

#endif  // VESPERAOS_SMART_DEVICE_H
