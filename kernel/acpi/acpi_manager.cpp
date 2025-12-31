#include "acpi_manager.h"

namespace ACPI
{
    // Static member initialization
    SDTHeader* TableManager::xsdt = nullptr;
    FADT* TableManager::fadt = nullptr;
    MADTHeader* TableManager::madt = nullptr;
    MCFGHeader* TableManager::mcfg = nullptr;

    void TableManager::init(const BootInfo* boot_info)
    {
        xsdt = reinterpret_cast<SDTHeader*>(boot_info->rsdp->xsdt_address);

        // Find and cache all known tables
        madt = reinterpret_cast<MADTHeader*>(find_table("APIC"));
        mcfg = reinterpret_cast<MCFGHeader*>(find_table("MCFG"));
        fadt = reinterpret_cast<FADT*>(find_table("FACP"));
    }

    SDTHeader* TableManager::find_table(const char* signature)
    {
        if (!xsdt) return nullptr;

        const uint32_t entry_count = (xsdt->length - sizeof(SDTHeader)) / 8;
        const auto* entries = reinterpret_cast<uint64_t*>(
            reinterpret_cast<uint64_t>(xsdt) + sizeof(SDTHeader)
        );

        for (uint32_t i = 0; i < entry_count; i++)
        {
            auto* header = reinterpret_cast<SDTHeader*>(entries[i]);

            // Compare signature (4 bytes)
            if (header->signature[0] == signature[0] &&
                header->signature[1] == signature[1] &&
                header->signature[2] == signature[2] &&
                header->signature[3] == signature[3])
            {
                return header;
            }
        }

        return nullptr;
    }
}