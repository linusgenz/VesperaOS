//
// Created by linus on 02.07.25.
//

#ifndef ACPI_MANAGER_H
#define ACPI_MANAGER_H
#include "acpi.h"
#include <boot.h>

namespace ACPI
{
    class TableManager
    {
    public:
        static void init(const BootInfo* boot_info);

        static FADT* get_fadt() { return fadt; }
        static MADTHeader* get_madt() { return madt; }
        static MCFGHeader* get_mcfg() { return mcfg; }

    private:
        static SDTHeader* find_table(const char* signature);

        static SDTHeader* xsdt;
        static FADT* fadt;
        static MADTHeader* madt;
        static MCFGHeader* mcfg;
    };
}

#endif //ACPI_MANAGER_H
