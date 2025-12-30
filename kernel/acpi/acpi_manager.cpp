#include "acpi_manager.h"

namespace ACPI
{
    FADT* TableManager::fadt = nullptr;
    MADTHeader* TableManager::madt = nullptr;
    MCFGHeader* TableManager::mcfg = nullptr;

    constexpr int MAX_TABLES = 16;

    struct TableEntry
    {
        const char* signature;
        SDTHeader* header;
    };

    SDTHeader* xsdt_ptr = nullptr;
    TableEntry table_list[MAX_TABLES];
    int table_count = 0;

    SDTHeader* find_table(const char* signature)
    {
        const uint32_t entries = (xsdt_ptr->length - sizeof(SDTHeader)) / 8;

        for (int t = 0; t < entries; t++)
        {
            const auto base = reinterpret_cast<uint64_t>(xsdt_ptr);
            const auto entry_addr = base + sizeof(SDTHeader) + t * 8;

            const auto entry_ptr = reinterpret_cast<uint64_t*>(entry_addr);
            const uint64_t entry = *entry_ptr;

            auto* new_sdt_header = reinterpret_cast<SDTHeader*>(entry);
            for (int i = 0; i < 4; i++)
            {
                if (new_sdt_header->signature[i] != signature[i])
                {
                    break;
                }
                if (i == 3) return new_sdt_header;
            }
        }
        return nullptr;
    }

    void TableManager::init(SDTHeader* xsdt)
    {
        xsdt_ptr = xsdt;
        table_count = 0;
    }

    void TableManager::register_madt()
    {
        madt = reinterpret_cast<MADTHeader*>(find_table("APIC"));
    }

    void TableManager::register_mcfg()
    {
        mcfg = reinterpret_cast<MCFGHeader*>(find_table("MCFG"));
    }

    void TableManager::register_fadr()
    {
        fadt = reinterpret_cast<FADT*>(find_table("FACP"));
    }

    FADT* TableManager::get_fadt()
    {
        return fadt;
    }

    MADTHeader* TableManager::get_madt()
    {
        return madt;
    }

    MCFGHeader* TableManager::get_mcfg()
    {
        return mcfg;
    }
}
