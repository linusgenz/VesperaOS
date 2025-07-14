//
// Created by linus on 06.10.24.
//

#ifndef PCI_H
#define PCI_H
#include <stdint.h>
#include "../../kernel/acpi/acpi.h"
#include "../../kernel/include/page_table_manager.h"
#include "../../kernel/include/basic_renderer.h"
#include "../../include/string.h"

namespace PCI {
    struct PCIDeviceHeader {
        uint16_t vendor_id;
        uint16_t device_id;
        volatile uint16_t command;
        volatile uint16_t status;
        uint8_t revision_id;
        uint8_t prog_if;
        uint8_t subclass;
        uint8_t _class;
        uint8_t cache_line_size;
        uint8_t latency_timer;
        uint8_t header_type;
        uint8_t bist;
    };

    struct PCIHeader0 {
        PCIDeviceHeader header;
        uint32_t BAR0;
        uint32_t BAR1;
        uint32_t BAR2;
        uint32_t BAR3;
        uint32_t BAR4;
        uint32_t BAR5;
        uint32_t cardbus_cis_ptr;
        uint16_t subsystem_vendor_id;
        uint16_t subsystem_id;
        uint32_t expansion_rom_base_addr;
        uint8_t capabilities_ptr;
        uint8_t rsv0;
        uint16_t rsv1;
        uint32_t rsv2;
        uint8_t interrupt_line;
        uint8_t interrupt_pin;
        uint8_t min_grant;
        uint8_t max_latency;
    };
    

    void enumerate_pci(ACPI::MCFGHeader* mcfg);

    extern const char* DeviceClasses[];

    const char* get_vendor_name(uint16_t vendor_id);

    const char* get_device_name(uint16_t vendor_id, uint16_t device_id);

    const char* get_subclass_name(uint8_t class_code, uint8_t subclass_code);

    const char* get_prog_if_Name(uint8_t class_code, uint8_t subclass_code, uint8_t prog_if);
}

#endif //PCI_H