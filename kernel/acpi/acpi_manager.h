//
// Created by linus on 02.07.25.
//

#ifndef ACPI_MANAGER_H
#define ACPI_MANAGER_H
#include <acpi/acpi.h>
#include <vespera/boot/boot.h>

namespace acpi {
    inline u64 rsdp_phys = 0;

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
        static FADT* fadt_;
        static MADT_HEADER* madt_;
        static MCFG_HEADER* mcfg_;
    };
}  // namespace ACPI

#endif  // ACPI_MANAGER_H
