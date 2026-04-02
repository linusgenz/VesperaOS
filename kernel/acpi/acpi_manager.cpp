#include "acpi_manager.h"

#include "ec.h"
#include <drivers/power/power_driver.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

namespace acpi {
    FADT* TableManager::fadt_ = nullptr;
    MADT_HEADER* TableManager::madt_ = nullptr;
    MCFG_HEADER* TableManager::mcfg_ = nullptr;

    void TableManager::init(const BootInfo* boot_info) {
        rsdp_phys = phys_raw(virt_to_phys(make_virt(boot_info->rsdp)));

        ACPI_STATUS status = AcpiInitializeSubsystem();
        if (ACPI_FAILURE(status)) {
            Log::error("ACPICA: AcpiInitializeSubsystem failed: %u", status);
            return;
        }

        status = AcpiInitializeTables(nullptr, 16, FALSE);
        if (ACPI_FAILURE(status)) {
            Log::error("ACPICA: AcpiInitializeTables failed: %u", status);
            return;
        }

        AcpiGetTable(ACPI_SIG_MADT, 1, reinterpret_cast<ACPI_TABLE_HEADER**>(&madt_));
        AcpiGetTable(ACPI_SIG_FADT, 1, reinterpret_cast<ACPI_TABLE_HEADER**>(&fadt_));
        AcpiGetTable(ACPI_SIG_MCFG, 1, reinterpret_cast<ACPI_TABLE_HEADER**>(&mcfg_));

        status = AcpiLoadTables();
        if (ACPI_FAILURE(status)) {
            Log::error("ACPICA: AcpiLoadTables failed: %u", status);
            return;
        }

        status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
        if (ACPI_FAILURE(status)) {
            Log::error("ACPICA: AcpiEnableSubsystem failed: %u", status);
            return;
        }

        ec::install_handler();

        status = AcpiInitializeObjects(ACPI_NO_DEVICE_INIT);
        if (ACPI_FAILURE(status)) {
            Log::error("ACPICA: AcpiInitializeObjects failed: %u", status);
            return;
        }

        Log::ok("ACPICA initialized");

        power::init();
    }
}  // namespace acpi