#include "acpi_manager.h"

#include <kernel/memory.h>

namespace acpi {
    // Static member initialization
    SDT_HEADER* TableManager::xsdt_ = nullptr;
    FADT* TableManager::fadt_ = nullptr;
    MADT_HEADER* TableManager::madt_ = nullptr;
    MCFG_HEADER* TableManager::mcfg_ = nullptr;

    void TableManager::init(const BootInfo* boot_info) {
        xsdt_ = static_cast<SDT_HEADER*>(virt_ptr(phys_to_virt(make_phys(boot_info->rsdp->xsdt_address))));

        // Find and cache all known tables
        madt_ = reinterpret_cast<MADT_HEADER*>(find_table("APIC"));
        mcfg_ = reinterpret_cast<MCFG_HEADER*>(find_table("MCFG"));
        fadt_ = reinterpret_cast<FADT*>(find_table("FACP"));
    }

    SDT_HEADER* TableManager::find_table(const char* signature) {
        if (!xsdt_) return nullptr;

        const uint32_t entry_count = (xsdt_->length - sizeof(SDT_HEADER)) / 8;
        const auto* entries = reinterpret_cast<uint64_t*>(reinterpret_cast<uint64_t>(xsdt_) + sizeof(SDT_HEADER));

        for (uint32_t i = 0; i < entry_count; i++) {
            auto* header = static_cast<SDT_HEADER*>(virt_ptr(phys_to_virt(make_phys(entries[i]))));
            if (memcmp(header->signature, signature, 4) == 0) return header;
        }

        return nullptr;
    }
}  // namespace ACPI
