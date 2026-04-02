// ec.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 02.04.26.
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

#include "ec.h"

#include <vespera/cpu/io.h>
#include <vespera/log.h>
#include <acpi/acpi.h>

namespace acpi::ec {

static u16 s_data_port    = 0x62;
static u16 s_command_port = 0x66;

static constexpr u32 EC_TIMEOUT_US = 10000;  // 10 ms

// EC status register bits
static constexpr u8 EC_OBF = (1 << 0);  // output buffer full
static constexpr u8 EC_IBF = (1 << 1);  // input buffer full

// EC commands
static constexpr u8 EC_CMD_READ  = 0x80;
static constexpr u8 EC_CMD_WRITE = 0x81;

static bool wait_ibf_clear() {
    for (u32 i = 0; i < EC_TIMEOUT_US; i++) {
        if (!(inb(s_command_port) & EC_IBF)) return true;
        asm volatile("pause");
    }
    Log::error("EC: IBF timeout");
    return false;
}

static bool wait_obf_set() {
    for (u32 i = 0; i < EC_TIMEOUT_US; i++) {
        if (inb(s_command_port) & EC_OBF) return true;
        asm volatile("pause");
    }
    Log::error("EC: OBF timeout");
    return false;
}

static bool ec_read(u8 offset, u8& value) {
    if (!wait_ibf_clear()) return false;
    outb(s_command_port, EC_CMD_READ);

    if (!wait_ibf_clear()) return false;
    outb(s_data_port, offset);

    if (!wait_obf_set()) return false;
    value = inb(s_data_port);
    return true;
}

static bool ec_write(u8 offset, u8 value) {
    if (!wait_ibf_clear()) return false;
    outb(s_command_port, EC_CMD_WRITE);

    if (!wait_ibf_clear()) return false;
    outb(s_data_port, offset);

    if (!wait_ibf_clear()) return false;
    outb(s_data_port, value);
    return true;
}

static ACPI_STATUS ec_space_handler(
    UINT32                  function,
    ACPI_PHYSICAL_ADDRESS   address,
    UINT32                  bit_width,
    UINT64*                 value,
    void*                   /*handler_context*/,
    void*                   /*region_context*/
) {
    const u8  offset     = static_cast<u8>(address);
    const u32 byte_count = bit_width / 8;

    if (function == ACPI_READ) {
        *value = 0;
        for (u32 i = 0; i < byte_count; i++) {
            u8 byte = 0;
            if (!ec_read(offset + static_cast<u8>(i), byte))
                return AE_TIME;
            *value |= static_cast<UINT64>(byte) << (i * 8);
        }
    } else {
        for (u32 i = 0; i < byte_count; i++) {
            const u8 byte = static_cast<u8>(*value >> (i * 8));
            if (!ec_write(offset + static_cast<u8>(i), byte))
                return AE_TIME;
        }
    }

    return AE_OK;
}

static ACPI_STATUS on_ec_found(
    ACPI_HANDLE object,
    UINT32      /*nesting_level*/,
    void*       /*context*/,
    void**      /*return_value*/
) {
    ACPI_BUFFER buf = { ACPI_ALLOCATE_BUFFER, nullptr };
    if (ACPI_SUCCESS(AcpiGetCurrentResources(object, &buf)) && buf.Pointer) {
        auto* res = static_cast<ACPI_RESOURCE*>(buf.Pointer);

        u16 ports[2] = { 0, 0 };
        int port_idx = 0;

        while (res->Type != ACPI_RESOURCE_TYPE_END_TAG && port_idx < 2) {
            if (res->Type == ACPI_RESOURCE_TYPE_IO) {
                ports[port_idx++] = static_cast<u16>(res->Data.Io.Minimum);
            }
            res = ACPI_NEXT_RESOURCE(res);
        }

        // _CRS lists two IO resources: first = data (0x62), second = cmd (0x66)
        if (port_idx == 2) {
            s_data_port    = ports[0];
            s_command_port = ports[1];
            Log::info("EC: ports from _CRS - data=0x%x, cmd=0x%x",
                      s_data_port, s_command_port);
        }

        AcpiOsFree(buf.Pointer);
    }

    const ACPI_STATUS status = AcpiInstallAddressSpaceHandler(
        object,
        ACPI_ADR_SPACE_EC,
        ec_space_handler,
        nullptr,
        nullptr
    );

    if (ACPI_FAILURE(status)) {
        Log::error("EC: AcpiInstallAddressSpaceHandler failed: %u", status);
    } else {
        Log::ok("EC: address space handler installed");
    }

    return AE_OK;
}

void install_handler() {
    const ACPI_STATUS status = AcpiGetDevices(
        const_cast<char*>("PNP0C09"),
        on_ec_found,
        nullptr,
        nullptr
    );

    if (ACPI_FAILURE(status)) {
        Log::error("EC: AcpiGetDevices(PNP0C09) failed: %u", status);
    }
}

}  // namespace acpi::ec