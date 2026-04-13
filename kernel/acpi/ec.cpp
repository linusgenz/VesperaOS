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
#include <vespera/sync/semaphore.h>
#include <vespera/time.h>
#include <vespera/unit_config.h>

#include "../units/unit_manager.h"
#include "klib/string.h"

namespace acpi::ec {

    static constexpr u8 EC_SC_OBF = (1u << 0);      // Output Buffer Full
    static constexpr u8 EC_SC_IBF = (1u << 1);      // Input Buffer Full
    static constexpr u8 EC_SC_SCI_EVT = (1u << 5);  // SCI Event pending

    // EC commands (ACPI Spec 12.4)
    static constexpr u8 EC_CMD_READ = 0x80;
    static constexpr u8 EC_CMD_WRITE = 0x81;
    static constexpr u8 EC_CMD_QUERY = 0x84;  // QR_EC

    static constexpr u64 EC_POLL_TIMEOUT_MS = 500;

    // Maximale Queries pro Worker-Durchlauf (Schutz gegen Event-Akkumulation)
    static constexpr int EC_MAX_DRAIN = 16;

    static Spinlock s_ec_lock;
    static Semaphore s_gpe_sem;

    static u16 s_data_port = 0x62;
    static u16 s_cmd_port = 0x66;
    static u32 s_ec_gpe = 0x16;
    static ACPI_HANDLE s_ec_handle = nullptr;

    static u8 ec_read_status() {
        return inb(s_cmd_port);
    }

    static bool wait_ibf_clear() {
        const u64 deadline = kernel::time::get_uptime_ms() + EC_POLL_TIMEOUT_MS;
        while (kernel::time::get_uptime_ms() < deadline) {
            if (!(ec_read_status() & EC_SC_IBF)) return true;
            asm volatile("pause");
        }
        Log::error("EC: IBF timeout");
        return false;
    }

    static bool wait_obf_set() {
        const u64 deadline = kernel::time::get_uptime_ms() + EC_POLL_TIMEOUT_MS;
        while (kernel::time::get_uptime_ms() < deadline) {
            if (ec_read_status() & EC_SC_OBF) return true;
            asm volatile("pause");
        }
        Log::error("EC: OBF timeout");
        return false;
    }

    // Leert den Output-Buffer des EC, falls er beim Start bereits Daten enthält.
    // Muss vor der ersten Transaktion aufgerufen werden.
    static void ec_drain_obf() {
        for (int i = 0; i < 256; i++) {
            if (!(ec_read_status() & EC_SC_OBF)) break;
            inb(s_data_port);
            AcpiOsStall(10);
        }
    }

    // -------------------------------------------------------------------------
    // EC-Protokoll
    // -------------------------------------------------------------------------

    static bool ec_read(u8 offset, u8& value) {
        SpinlockGuard g(s_ec_lock);
        if (!wait_ibf_clear()) return false;
        outb(s_cmd_port, EC_CMD_READ);
        if (!wait_ibf_clear()) return false;
        outb(s_data_port, offset);
        if (!wait_obf_set()) return false;
        value = inb(s_data_port);
        return true;
    }

    static bool ec_write(u8 offset, u8 value) {
        SpinlockGuard g(s_ec_lock);
        if (!wait_ibf_clear()) return false;
        outb(s_cmd_port, EC_CMD_WRITE);
        if (!wait_ibf_clear()) return false;
        outb(s_data_port, offset);
        if (!wait_ibf_clear()) return false;
        outb(s_data_port, value);
        return true;
    }

    static u8 ec_query() {
        SpinlockGuard g(s_ec_lock);
        if (!wait_ibf_clear()) return 0;
        outb(s_cmd_port, EC_CMD_QUERY);

        for (int retry = 0; retry < 3; retry++) {
            if (wait_obf_set()) return inb(s_data_port);
        }
        return 0;
    }

    static void ec_dispatch_query(u8 query_code) {
        if (query_code == 0) return;

        char method[5];
        snprintf(method, sizeof(method), "_Q%02X", query_code);

        const ACPI_STATUS st = AcpiEvaluateObject(s_ec_handle, method, nullptr, nullptr);
        if (ACPI_FAILURE(st) && st != AE_NOT_FOUND) {
            Log::warning("EC: %s: %s", method, AcpiFormatException(st));
        }
    }

    static bool ec_drain_queries() {
        bool handled_any = false;

        for (int i = 0; i < EC_MAX_DRAIN; i++) {
            // Prüfe zuerst ob noch SCI_EVT gesetzt ist (ohne Lock, nur Hint)
            if (!(ec_read_status() & EC_SC_SCI_EVT)) break;

            const u8 query_code = ec_query();
            if (query_code == 0) break;

            ec_dispatch_query(query_code);
            handled_any = true;
        }

        return handled_any;
    }

    static void ec_worker_thread(void* /*arg*/) {
        while (true) {
            s_gpe_sem.wait(0xFFFF);

            ec_drain_queries();

            const ACPI_STATUS st = AcpiFinishGpe(nullptr, s_ec_gpe);
            if (ACPI_FAILURE(st)) {
                Log::error("EC: AcpiFinishGpe failed: %s", AcpiFormatException(st));
                AcpiEnableGpe(nullptr, s_ec_gpe);
            }
        }
    }

    static UINT32 ec_gpe_handler(ACPI_HANDLE /*device*/, UINT32 /*gpe*/, void* /*ctx*/) {
        const u8 status = ec_read_status();
        if (!(status & EC_SC_SCI_EVT)) {
            AcpiFinishGpe(nullptr, s_ec_gpe);
            return ACPI_INTERRUPT_HANDLED;
        }

        s_gpe_sem.signal(1);

        return ACPI_INTERRUPT_HANDLED;
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

    static ACPI_STATUS on_ec_found(ACPI_HANDLE object, UINT32 /*level*/, void* /*ctx*/, void** /*ret*/) {
        // I/O-Ports aus _CRS lesen
        {
            ACPI_BUFFER buf = {ACPI_ALLOCATE_BUFFER, nullptr};
            if (ACPI_SUCCESS(AcpiGetCurrentResources(object, &buf)) && buf.Pointer) {
                auto* res = static_cast<ACPI_RESOURCE*>(buf.Pointer);
                u16 ports[2] = {0, 0};
                int port_idx = 0;

                while (res->Type != ACPI_RESOURCE_TYPE_END_TAG && port_idx < 2) {
                    if (res->Type == ACPI_RESOURCE_TYPE_IO) {
                        ports[port_idx++] = res->Data.Io.Minimum;
                    }
                    res = ACPI_NEXT_RESOURCE(res);
                }

                if (port_idx == 2) {
                    s_data_port = ports[0];
                    s_cmd_port = ports[1];
                    Log::info("EC: _CRS ports — data=0x%x, cmd=0x%x", s_data_port, s_cmd_port);
                } else {
                    Log::warning("EC: _CRS incomplete (%d ports found), using defaults 0x62/0x66", port_idx);
                }
                AcpiOsFree(buf.Pointer);
            }
        }

        // GPE-Nummer aus _GPE lesen
        {
            ACPI_BUFFER gpe_buf = {ACPI_ALLOCATE_BUFFER, nullptr};
            if (ACPI_SUCCESS(AcpiEvaluateObject(object, "_GPE", nullptr, &gpe_buf)) && gpe_buf.Pointer) {
                const auto* obj = static_cast<ACPI_OBJECT*>(gpe_buf.Pointer);
                if (obj->Type == ACPI_TYPE_INTEGER) {
                    s_ec_gpe = static_cast<u32>(obj->Integer.Value);
                }
                AcpiOsFree(gpe_buf.Pointer);
            }
        }

        Log::info("EC: GPE=%u (0x%x)", s_ec_gpe, s_ec_gpe);

        ec_drain_obf();
        for (int i = 0; i < 100; i++) {
            if (!(ec_read_status() & EC_SC_IBF)) break;
            AcpiOsStall(10);
        }

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
        s_ec_lock.init("ec_lock");
        s_gpe_sem.init(32, 0);

        const ACPI_STATUS st = AcpiGetDevices(const_cast<char*>("PNP0C09"), on_ec_found, nullptr, nullptr);
        if (ACPI_FAILURE(st)) {
            Log::error("EC: install_space_handler failed: %s", AcpiFormatException(st));
        }
    }

    void install_gpe_handler() {
        if (!s_ec_handle) {
            Log::error("EC: install_gpe_handler called before install_space_handler");
            return;
        }

        static constexpr UnitConfig kEcWorkerCfg = {
            .name = "ec_worker",
            .cpu_id = 7,
            .priority = 5,
            .stack_size = 0x4000,
            .initial_handles = nullptr,
            .initial_handle_count = 0,
            .is_idle = false,
            .is_user = false,
            .user_stack_size = 0,
            .auto_schedule = true,
            .argv = nullptr,
            .envp = nullptr,
        };
        UnitManager::create(KERNEL_REALM_SYSTEM, ec_worker_thread, nullptr, &kEcWorkerCfg);

        ACPI_STATUS st = AcpiInstallGpeRawHandler(nullptr, s_ec_gpe, ACPI_GPE_LEVEL_TRIGGERED, ec_gpe_handler, nullptr);
        if (ACPI_FAILURE(st)) {
            Log::error("EC: AcpiInstallGpeRawHandler(%u) failed: %s", s_ec_gpe, AcpiFormatException(st));
            return;
        }
        Log::ok("EC: GPE %u raw handler installed (level-triggered)", s_ec_gpe);

        st = AcpiEnableGpe(nullptr, s_ec_gpe);
        if (ACPI_FAILURE(st)) {
            Log::error("EC: AcpiEnableGpe(%u) failed: %s", s_ec_gpe, AcpiFormatException(st));
        } else {
            Log::ok("EC: GPE %u enabled", s_ec_gpe);
        }
    }

}  // namespace acpi::ec