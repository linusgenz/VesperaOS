//
// Created by linus on 02.07.25.
//

#ifndef ACPI_MANAGER_H
#define ACPI_MANAGER_H
#include <boot.h>

#include "acpi.h"

namespace acpi {
    class TableManager {
       public:
        static void init(const BootInfo* boot_info);

        static FADT* get_fadt() {
            return fadt_;
        }
        static MADT_HEADER* get_madt() {
            return madt_;
        }
        static MCFG_HEADER* get_mcfg() {
            return mcfg_;
        }

       private:
        static SDT_HEADER* find_table(const char* signature);

        static SDT_HEADER* xsdt_;
        static FADT* fadt_;
        static MADT_HEADER* madt_;
        static MCFG_HEADER* mcfg_;
    };
}  // namespace ACPI

#endif  // ACPI_MANAGER_H
