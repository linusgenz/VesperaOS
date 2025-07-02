//
// Created by linus on 30.06.25.
//

#ifndef MADT_H
#define MADT_H
#include "acpi.h"
namespace MADT {
    void parse_madt(ACPI::MADTHeader* madt);
}
#endif //MADT_H
