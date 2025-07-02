//
// Created by linus on 02.07.25.
//

#ifndef ACPI_MANAGER_H
#define ACPI_MANAGER_H
#include "acpi.h"
#include <stdint.h>
namespace ACPI {

    class TableManager {
    public:
        static void init(SDTHeader* xsdt);
        static void register_madt();
        static void register_mcfg();
        static void register_fadr();

        static FADT* get_fadt();
        static MADTHeader* get_madt();
        static MCFGHeader* get_mcfg();

    private:
        static FADT* fadt;
        static MADTHeader* madt;
        static MCFGHeader* mcfg ;
    };

}

#endif //ACPI_MANAGER_H
