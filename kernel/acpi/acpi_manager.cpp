#include "acpi_manager.h"
#include "../../include/string.h"
#include "../../kernel/include/basic_renderer.h"

namespace ACPI {
    FADT* TableManager::fadt = nullptr;
    MADTHeader* TableManager::madt = nullptr;
    MCFGHeader* TableManager::mcfg = nullptr;

    constexpr int MAX_TABLES = 16;

    struct TableEntry {
        const char *signature;
        ACPI::SDTHeader *header;
    };

    SDTHeader *xsdt_ptr = nullptr;
    TableEntry table_list[MAX_TABLES];
    int table_count = 0;

    SDTHeader* find_table(const char* signature) {
        global_renderer->print(to_hstring(xsdt_ptr));
        int entries = (xsdt_ptr->length - sizeof(SDTHeader)) / 8;

        for (int t = 0; t < entries; t++) {
            SDTHeader* new_sdt_header = (SDTHeader*)*(uint64_t*)((uint64_t)xsdt_ptr + sizeof(SDTHeader) + (t * 8));
            for (int i = 0; i < 4; i++) {
                if (new_sdt_header->signature[i] != signature[i]) {
                    break;
                }
                if (i == 3) return new_sdt_header;
            }
        }
        return nullptr;
    }

    void TableManager::init(SDTHeader *xsdt) {
        xsdt_ptr = xsdt;
        table_count = 0;
        global_renderer->print("ACPI: TableManager initialisiert");
        global_renderer->new_line();
    }

    void TableManager::register_madt() {
        madt =  reinterpret_cast<MADTHeader *>(find_table("APIC"));
    }

    void TableManager::register_mcfg() {
        mcfg =  reinterpret_cast<MCFGHeader *>(find_table("MCFG"));
    }

    void TableManager::register_fadr() {
        fadt =  reinterpret_cast<FADT *>(find_table("FACP"));
    }

    FADT* TableManager::get_fadt() {
        return fadt;
    }

    MADTHeader* TableManager::get_madt() {
        return madt;
    }

    MCFGHeader* TableManager::get_mcfg() {
        return mcfg;
    }

}
