//
// Created by linus on 30.06.25.
//

#include "acpi.h"
#include "../../arch/x86_64/interrupts/apic.h"
#include "../../arch/x86_64/interrupts/interrupts.h"
#include "../../include/string.h"
#include "../include/basic_renderer.h"

namespace MADT {
    void parse_madt(ACPI::MADTHeader* madt) {
        bool has_legacy_pic = (madt->flags  & 0x1) != 0;
        if (has_legacy_pic) {
            global_renderer->print("HAT PIC!");
            global_renderer->new_line();
        } else {
            global_renderer->print("HAT KEIN PIC!");
            global_renderer->new_line();
        }

        LAPIC_ADDRESS = madt->lapic_address;

        uint8_t* entries = (uint8_t*)madt + sizeof(ACPI::MADTHeader);
        uint8_t* end = (uint8_t*)madt + madt->header.length;

        while (entries < end) {
            ACPI::MADTEntryHeader* header = (ACPI::MADTEntryHeader*)entries;

            switch ((ACPI::MADTEntryType)header->type) {
                case ACPI::MADTEntryType::IO_APIC: {
                    auto* entry = (ACPI::IOAPICEntry*)entries;
                    // IO APIC Adresse, GSI base etc. speichern
                    break;
                }
                case ACPI::MADTEntryType::INTERRUPT_OVERRIDE: {
                    auto* entry = (ACPI::InterruptOverrideEntry*)entries;
                    // IRQ → GSI mapping merken
                    break;
                }
                default:
                    break;
            }

            entries += header->length;
        }
    }

}
