// acpi_subsystem.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 30.06.25.
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

#include "acpi_subsystem.h"

#include <acpi/acpi.h>
#include <acpi/madt.h>
#include <drivers/power/power_driver.h>
#include <klib/string.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

#include "acpi_osl.h"
#include "acpi_tables.h"
#include "ec.h"

namespace kernel::acpi {

    namespace {
        u64 g_rsdp_phys = 0;
        FADT* g_fadt = nullptr;
        MADT_HEADER* g_madt = nullptr;
        MCFG_HEADER* g_mcfg = nullptr;
        HPET* g_hpet = nullptr;
    }  // namespace

    void early_init(const BootInfo* boot_info) {
        g_rsdp_phys = phys_raw(virt_to_phys(make_virt(boot_info->rsdp)));
        auto* rsdp = reinterpret_cast<RSDP2*>(boot_info->rsdp);

        MADT_HEADER* madt = nullptr;

        if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
            auto* xsdt = static_cast<XSDT*>(virt_ptr(phys_to_virt(make_phys(rsdp->xsdt_address))));
            const usize entry_count = (xsdt->header.length - sizeof(SDT_HEADER)) / sizeof(u64);

            for (usize i = 0; i < entry_count; ++i) {
                auto* sdt = static_cast<SDT_HEADER*>(virt_ptr(phys_to_virt(make_phys(xsdt->entries[i]))));
                if (memcmp(sdt->signature, "APIC", 4) == 0) {
                    madt = reinterpret_cast<MADT_HEADER*>(sdt);
                }
                if (memcmp(sdt->signature, "FACP", 4) == 0) {
                    g_fadt = reinterpret_cast<FADT*>(sdt);
                }
                if (memcmp(sdt->signature, "HPET", 4) == 0) {
                    g_hpet = reinterpret_cast<HPET*>(sdt);
                }
            }
        } else {
            auto* rsdt = static_cast<RSDT*>(virt_ptr(phys_to_virt(make_phys(rsdp->rsdt_address))));
            const usize entry_count = (rsdt->header.length - sizeof(SDT_HEADER)) / sizeof(u32);

            for (usize i = 0; i < entry_count; ++i) {
                auto* sdt = static_cast<SDT_HEADER*>(virt_ptr(phys_to_virt(make_phys(rsdt->entries[i]))));
                if (memcmp(sdt->signature, "APIC", 4) == 0) {
                    madt = reinterpret_cast<MADT_HEADER*>(sdt);
                }
                if (memcmp(sdt->signature, "FACP", 4) == 0) {
                    g_fadt = reinterpret_cast<FADT*>(sdt);
                }
                if (memcmp(sdt->signature, "HPET", 4) == 0) {
                    g_hpet = reinterpret_cast<HPET*>(sdt);
                }
            }
        }

        if (!madt) {
            Log::error("ACPI early_init: MADT not found");
            return;
        }

        madt::parse(madt);
        Log::ok("ACPI early_init: MADT parsed (%u CPUs, %u IOAPICs)", madt::cpu_count(), madt::ioapic_count());
    }

    void init() {
        AcpiDbgLayer = 0;
        AcpiDbgLevel = 0;
        acpi_osl_init_worker();

        ACPI_STATUS status = AcpiInitializeSubsystem();
        if (ACPI_FAILURE(status)) {
            Log::error("ACPICA: AcpiInitializeSubsystem failed: %s", AcpiFormatException(status));
            return;
        }

        status = AcpiInitializeTables(nullptr, 16, FALSE);
        if (ACPI_FAILURE(status)) {
            Log::error("ACPICA: AcpiInitializeTables failed: %s", AcpiFormatException(status));
            return;
        }

        AcpiGetTable(const_cast<ACPI_STRING>(ACPI_SIG_MADT), 1, reinterpret_cast<ACPI_TABLE_HEADER**>(&g_madt));
        AcpiGetTable(const_cast<ACPI_STRING>(ACPI_SIG_FADT), 1, reinterpret_cast<ACPI_TABLE_HEADER**>(&g_fadt));
        AcpiGetTable(const_cast<ACPI_STRING>(ACPI_SIG_MCFG), 1, reinterpret_cast<ACPI_TABLE_HEADER**>(&g_mcfg));
        AcpiGetTable(const_cast<ACPI_STRING>(ACPI_SIG_HPET), 1, reinterpret_cast<ACPI_TABLE_HEADER**>(&g_hpet));

        status = AcpiLoadTables();
        if (ACPI_FAILURE(status)) {
            Log::error("ACPICA: AcpiLoadTables failed: %s", AcpiFormatException(status));
            return;
        }

        status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
        if (ACPI_FAILURE(status)) {
            Log::error("ACPICA: AcpiEnableSubsystem failed: %s", AcpiFormatException(status));
            return;
        }

        ec::install_space_handler();

        status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
        if (ACPI_FAILURE(status)) {
            Log::error("ACPICA: AcpiInitializeObjects failed: %s", AcpiFormatException(status));
            return;
        }

        power::init();

        ec::install_gpe_handler();

        status = AcpiUpdateAllGpes();
        if (ACPI_FAILURE(status)) {
            Log::warning("ACPICA: AcpiUpdateAllGpes failed: %s (events may not work)", AcpiFormatException(status));
        } else {
            Log::ok("ACPICA: runtime GPEs enabled");
        }

        AcpiInstallFixedEventHandler(
            ACPI_EVENT_POWER_BUTTON,
            [](void*) -> UINT32 {
                Log::info("ACPI: power button event received");
                return ACPI_INTERRUPT_HANDLED;
            },
            nullptr
        );
        AcpiEnableEvent(ACPI_EVENT_POWER_BUTTON, 0);

        Log::ok("ACPICA initialized");
    }

    u64 get_rsdp_phys() {
        return g_rsdp_phys;
    }

    FADT* get_fadt() {
        return g_fadt;
    }

    MADT_HEADER* get_madt() {
        return g_madt;
    }

    MCFG_HEADER* get_mcfg() {
        return g_mcfg;
    }

    HPET* get_hpet() {
        return g_hpet;
    }

}  // namespace kernel::acpi