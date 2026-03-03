#include "acpi_manager.h"

#include <kernel/memory.h>

namespace ACPI {
    // Static member initialization
    SDTHeader* TableManager::xsdt = nullptr;
    FADT* TableManager::fadt = nullptr;
    MADTHeader* TableManager::madt = nullptr;
    MCFGHeader* TableManager::mcfg = nullptr;

    void TableManager::init(const BootInfo* boot_info) {
        xsdt = static_cast<SDTHeader*>(virt_ptr(phys_to_virt(make_phys(boot_info->rsdp->xsdt_address))));

        // Find and cache all known tables
        madt = reinterpret_cast<MADTHeader*>(find_table("APIC"));
        mcfg = reinterpret_cast<MCFGHeader*>(find_table("MCFG"));
        fadt = reinterpret_cast<FADT*>(find_table("FACP"));
    }

    SDTHeader* TableManager::find_table(const char* signature) {
        if (!xsdt) return nullptr;

        const uint32_t entry_count = (xsdt->length - sizeof(SDTHeader)) / 8;
        const auto* entries = reinterpret_cast<uint64_t*>(reinterpret_cast<uint64_t>(xsdt) + sizeof(SDTHeader));

        for (uint32_t i = 0; i < entry_count; i++) {
            auto* header = static_cast<SDTHeader*>(virt_ptr(phys_to_virt(make_phys(entries[i]))));
            if (memcmp(header->signature, signature, 4) == 0) return header;
        }

        return nullptr;
    }
}  // namespace ACPI
