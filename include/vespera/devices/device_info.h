// device_info.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 11.03.26.
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
#ifndef VESPERAOS_DEVICE_INFO_H
#define VESPERAOS_DEVICE_INFO_H

#include <vespera/types.h>

class IDeviceInfo {
public:
    virtual bool get_model(char* out, usize len)    { out[0] = '\0'; return false; }
    virtual bool get_serial(char* out, usize len)   { out[0] = '\0'; return false; }
    virtual bool get_vendor(char* out, usize len)   { out[0] = '\0'; return false; }
    virtual bool get_firmware(char* out, usize len) { out[0] = '\0'; return false; }
    virtual ~IDeviceInfo() = default;
};

#endif  // VESPERAOS_DEVICE_INFO_H
