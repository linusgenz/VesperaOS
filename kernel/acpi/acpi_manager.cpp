#include "acpi_manager.h"

#include <acpi/acpi.h>
#include <drivers/power/power_driver.h>
#include <klib/string.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

#include "ec.h"
#include "madt.h"
#include "osl/acpi_osl.h"

namespace acpi {
    FADT* TableManager::fadt_ = nullptr;
    MADT_HEADER* TableManager::madt_ = nullptr;
    MCFG_HEADER* TableManager::mcfg_ = nullptr;

    void early_parse_madt(const BootInfo* boot_info) {
        auto* rsdp = reinterpret_cast<RSDP2*>(boot_info->rsdp);

        MADT_HEADER* madt = nullptr;

        if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
            auto* xsdt = static_cast<XSDT*>(virt_ptr(phys_to_virt(make_phys(rsdp->xsdt_address))));
            const usize entry_count = (xsdt->header.length - sizeof(SDT_HEADER)) / sizeof(u64);

            for (usize i = 0; i < entry_count; ++i) {
                auto* sdt = static_cast<SDT_HEADER*>(virt_ptr(phys_to_virt(make_phys(xsdt->entries[i]))));
                if (memcmp(sdt->signature, "APIC", 4) == 0) {
                    madt = reinterpret_cast<acpi::MADT_HEADER*>(sdt);
                    break;
                }
            }
        } else {
            auto* rsdt = static_cast<RSDT*>(virt_ptr(phys_to_virt(make_phys(rsdp->rsdt_address))));
            const usize entry_count = (rsdt->header.length - sizeof(SDT_HEADER)) / sizeof(u32);

            for (usize i = 0; i < entry_count; ++i) {
                auto* sdt = static_cast<SDT_HEADER*>(virt_ptr(phys_to_virt(make_phys(rsdt->entries[i]))));
                if (memcmp(sdt->signature, "APIC", 4) == 0) {
                    madt = reinterpret_cast<MADT_HEADER*>(sdt);
                    break;
                }
            }
        }

        if (!madt) {
            Log::error("ACPI early init: MADT not found!");
            return;
        }

        madt::parse_madt(madt);
        Log::ok("ACPI early init: MADT parsed (%u CPUs, %u IOAPICs)", madt::get_cpu_count(), madt::get_ioapic_count());
    }

    SDT_HEADER* TableManager::find_table(const char* signature) {
        auto* rsdp = static_cast<RSDP2*>(virt_ptr(phys_to_virt(make_phys(rsdp_phys))));
        auto* xsdt = static_cast<XSDT*>(virt_ptr(phys_to_virt(make_phys(rsdp->xsdt_address))));
        const u32 entry_count = (xsdt->header.length - sizeof(SDT_HEADER)) / 8;
        const auto* entries = reinterpret_cast<u64*>(reinterpret_cast<u64>(xsdt) + sizeof(SDT_HEADER));

        for (u32 i = 0; i < entry_count; i++) {
            auto* header = static_cast<SDT_HEADER*>(virt_ptr(phys_to_virt(make_phys(entries[i]))));
            if (memcmp(header->signature, signature, 4) == 0) return header;
        }
        return nullptr;
    }

    void TableManager::init() {
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

        AcpiGetTable(ACPI_SIG_MADT, 1, reinterpret_cast<ACPI_TABLE_HEADER**>(&madt_));
        AcpiGetTable(ACPI_SIG_FADT, 1, reinterpret_cast<ACPI_TABLE_HEADER**>(&fadt_));
        AcpiGetTable(ACPI_SIG_MCFG, 1, reinterpret_cast<ACPI_TABLE_HEADER**>(&mcfg_));

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
                Log::info("ACPI: Power button event received");
                return ACPI_INTERRUPT_HANDLED;
            },
            nullptr
        );
        AcpiEnableEvent(ACPI_EVENT_POWER_BUTTON, 0);

        Log::ok("ACPICA initialized");
    }

}  // namespace acpi