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

#include <acpi/acpi.h>
#include <vespera/cpu/io.h>
#include <vespera/log.h>

#include "klib/string.h"

namespace acpi::ec {

    static u16 s_data_port = 0x62;
    static u16 s_command_port = 0x66;
    static ACPI_HANDLE s_ec_handle = nullptr;
    static u32 s_ec_gpe = 0x16;

    static constexpr u32 EC_TIMEOUT_US = 10000;
    static constexpr u8 EC_OBF = (1 << 0);
    static constexpr u8 EC_IBF = (1 << 1);
    static constexpr u8 EC_CMD_READ = 0x80;
    static constexpr u8 EC_CMD_WRITE = 0x81;
    static constexpr u8 EC_CMD_QUERY = 0x84;

    // Drains any stale OBF bytes and waits for IBF to clear.
    static void ec_reset_state() {
        for (int i = 0; i < 256; i++) {
            if (!(inb(s_command_port) & EC_OBF)) break;
            inb(s_data_port);
            asm volatile("pause");
        }
        for (u32 i = 0; i < EC_TIMEOUT_US; i++) {
            if (!(inb(s_command_port) & EC_IBF)) break;
            asm volatile("pause");
        }
    }

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

    static bool ec_query_pending(u8& query_code) {
        if (!wait_ibf_clear()) return false;
        outb(s_command_port, EC_CMD_QUERY);
        if (!wait_obf_set()) return false;
        query_code = inb(s_data_port);
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

    static void ec_dispatch_query(u8 query_code) {
        if (query_code == 0) return;

        char method[5];
        snprintf(method, sizeof(method), "_Q%02X", query_code);

        const ACPI_STATUS st = AcpiEvaluateObject(s_ec_handle, method, nullptr, nullptr);
        if (ACPI_FAILURE(st) && st != AE_NOT_FOUND) {
            Log::warning("EC: %s failed: %s", method, AcpiFormatException(st));
        }
    }

    struct EcQueryTask {
        u8 query_code;
    };

    static void ec_query_task(void* ctx) {
        auto* task = static_cast<EcQueryTask*>(ctx);
        ec_dispatch_query(task->query_code);
        AcpiOsFree(task);
    }

    // Installed via AcpiInstallGpeRawHandler so ACPICA never touches this
    // GPE entry again — it will not auto-disable it after the first firing.
    static UINT32 ec_gpe_handler(ACPI_HANDLE /*device*/, UINT32 /*gpe*/, void* /*ctx*/) {
        u8 query_code = 0;
        if (!ec_query_pending(query_code)) {
            return ACPI_INTERRUPT_HANDLED | ACPI_REENABLE_GPE;
        }

        if (query_code != 0) {
            auto* task = static_cast<EcQueryTask*>(AcpiOsAllocate(sizeof(EcQueryTask)));
            if (task) {
                task->query_code = query_code;
                AcpiOsExecute(OSL_EC_POLL_HANDLER, ec_query_task, task);
            } else {
                Log::error("EC: GPE handler OOM");
            }
        }

        return ACPI_INTERRUPT_HANDLED | ACPI_REENABLE_GPE;
    }

    static ACPI_STATUS ec_space_handler(
        UINT32 function, ACPI_PHYSICAL_ADDRESS address, UINT32 bit_width, UINT64* value, void* /*handler_ctx*/,
        void* /*region_ctx*/
    ) {
        const u8 offset = static_cast<u8>(address);
        const u32 byte_count = bit_width / 8;

        if (function == ACPI_READ) {
            *value = 0;
            for (u32 i = 0; i < byte_count; i++) {
                u8 byte = 0;
                if (!ec_read(offset + static_cast<u8>(i), byte)) return AE_TIME;
                *value |= static_cast<UINT64>(byte) << (i * 8);
            }
        } else {
            for (u32 i = 0; i < byte_count; i++) {
                const u8 byte = static_cast<u8>(*value >> (i * 8));
                if (!ec_write(offset + static_cast<u8>(i), byte)) return AE_TIME;
            }
        }
        return AE_OK;
    }

    static ACPI_STATUS on_ec_found_phase1(ACPI_HANDLE object, UINT32, void*, void**) {
        ec_reset_state();

        // Read port addresses from _CRS
        {
            ACPI_BUFFER buf = {ACPI_ALLOCATE_BUFFER, nullptr};
            if (ACPI_SUCCESS(AcpiGetCurrentResources(object, &buf)) && buf.Pointer) {
                auto* res = static_cast<ACPI_RESOURCE*>(buf.Pointer);
                u16 ports[2] = {0, 0};
                int port_idx = 0;

                while (res->Type != ACPI_RESOURCE_TYPE_END_TAG && port_idx < 2) {
                    if (res->Type == ACPI_RESOURCE_TYPE_IO) ports[port_idx++] = res->Data.Io.Minimum;
                    res = ACPI_NEXT_RESOURCE(res);
                }
                if (port_idx == 2) {
                    s_data_port = ports[0];
                    s_command_port = ports[1];
                    Log::info("EC: ports from _CRS — data=0x%x, cmd=0x%x", s_data_port, s_command_port);
                }
                AcpiOsFree(buf.Pointer);
            }
        }

        // Read GPE number from _GPE
        {
            ACPI_BUFFER gpe_buf = {ACPI_ALLOCATE_BUFFER, nullptr};
            if (ACPI_SUCCESS(AcpiEvaluateObject(object, "_GPE", nullptr, &gpe_buf)) && gpe_buf.Pointer) {
                const auto* obj = static_cast<ACPI_OBJECT*>(gpe_buf.Pointer);
                if (obj->Type == ACPI_TYPE_INTEGER) s_ec_gpe = static_cast<u32>(obj->Integer.Value);
                AcpiOsFree(gpe_buf.Pointer);
            }
        }
        Log::info("EC: GPE = %u (0x%x)", s_ec_gpe, s_ec_gpe);

        s_ec_handle = object;

        const ACPI_STATUS st =
            AcpiInstallAddressSpaceHandler(object, ACPI_ADR_SPACE_EC, ec_space_handler, nullptr, nullptr);
        if (ACPI_FAILURE(st)) {
            Log::error("EC: AcpiInstallAddressSpaceHandler failed: %s", AcpiFormatException(st));
        } else {
            Log::ok("EC: address space handler installed");
        }

        return AE_OK;
    }

    void install_space_handler() {
        const ACPI_STATUS st = AcpiGetDevices(const_cast<char*>("PNP0C09"), on_ec_found_phase1, nullptr, nullptr);
        if (ACPI_FAILURE(st)) Log::error("EC: install_space_handler failed: %s", AcpiFormatException(st));
    }

    void install_gpe_handler() {
        if (!s_ec_handle) {
            Log::error("EC: install_gpe_handler called before install_space_handler");
            return;
        }

        ACPI_STATUS st = AcpiInstallGpeRawHandler(nullptr, s_ec_gpe, ACPI_GPE_EDGE_TRIGGERED, ec_gpe_handler, nullptr);
        if (ACPI_FAILURE(st)) {
            Log::error("EC: AcpiInstallGpeRawHandler(%u) failed: %s", s_ec_gpe, AcpiFormatException(st));
            return;
        }
        Log::ok("EC: GPE %u raw handler installed", s_ec_gpe);

        st = AcpiEnableGpe(nullptr, s_ec_gpe);
        if (ACPI_FAILURE(st))
            Log::error("EC: AcpiEnableGpe(%u) failed: %s", s_ec_gpe, AcpiFormatException(st));
        else
            Log::ok("EC: GPE %u enabled", s_ec_gpe);
    }

}  // namespace acpi::ec