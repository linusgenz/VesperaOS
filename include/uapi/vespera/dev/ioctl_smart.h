// ioctl_smart.h
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
#ifndef VESPERAOS_IOCTL_SMART_H
#define VESPERAOS_IOCTL_SMART_H

#include <vespera/types.h>

#define IOCTL_SMART_GET_RAW     0x5301  // → SmartRawData (512 bytes)
#define IOCTL_SMART_GET_ATTRS   0x5302  // → SmartAttributes

typedef struct SmartAttribute {
    u8  id;
    u16 flags;
    u8  current;    // normalisierter Wert (höher = besser)
    u8  worst;
    u8  raw[6];     // roher Wert, z.B. raw[0] = Temperatur in °C
    u8  reserved;
}__attribute__((packed)) SmartAttribute;

typedef struct SmartAttributes {
    u8             version;
    SmartAttribute attrs[30];  // max 30 Attribute
    u8             attr_count;

    // Geparste Convenience-Felder
    u8  temperature_celsius;   // Attr 0xBE
    u64 power_on_hours;        // Attr 0x09
    u32 reallocated_sectors;   // Attr 0x05
    u32 pending_sectors;       // Attr 0xC5
    u8  health_ok;             // 1 wenn keine kritischen Attribute
}SmartAttributes;

typedef struct SmartRawData {
    u8 data[512];
} SmartRawData;

#endif  // VESPERAOS_IOCTL_SMART_H
