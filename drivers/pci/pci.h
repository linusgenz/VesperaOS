//
// Created by linus on 06.10.24.
//

#ifndef PCI_H
#define PCI_H
#include <stdint.h>

#include <acpi/acpi.h>

namespace pci {
    struct PCI_DEVICE_HEADER {
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

    struct PCI_HEADER0 {
        PCI_DEVICE_HEADER header;
        uint32_t bar0;
        uint32_t bar1;
        uint32_t bar2;
        uint32_t bar3;
        uint32_t bar4;
        uint32_t bar5;
        uint32_t cardbus_cis_ptr;
        uint16_t subsystem_vendor_id;
        uint16_t subsystem_id;
        uint32_t expansion_robase_addr;
        uint8_t capabilities_ptr;
        uint8_t rsv0;
        uint16_t rsv1;
        uint32_t rsv2;
        uint8_t interrupt_line;
        uint8_t interrupt_pin;
        uint8_t min_grant;
        uint8_t max_latency;
    };

#define PCI_BAR_TYPE_32_BIT    0x0
#define PCI_BAR_TYPE_64_BIT    0x4
#define PCI_BAR_TYPE_MASK     0x6
#define PCI_BAR_MEMORY_MASK   0x1

    struct BarInfo {
        uint64_t address;
        uint64_t size;
        bool is_64_bit;
        bool is_memory;
        bool is_prefetchable;
        bool is_valid;
    };

    inline uint8_t pci_read8(pci::PCI_DEVICE_HEADER *hdr, uint8_t offset) {
        volatile uint8_t *ptr = reinterpret_cast<uint8_t *>(hdr) + offset;
        return *ptr;
    }

    inline void pci_write8(pci::PCI_DEVICE_HEADER *hdr, uint8_t offset, uint8_t value) {
        volatile uint8_t *ptr = reinterpret_cast<uint8_t *>(hdr) + offset;
        *ptr = value;
    }

    inline uint16_t pci_read16(pci::PCI_DEVICE_HEADER *hdr, uint8_t offset) {
        volatile auto *ptr = reinterpret_cast<volatile uint16_t *>(reinterpret_cast<uint8_t *>(hdr) + offset);
        return *ptr;
    }

    inline void pci_write16(pci::PCI_DEVICE_HEADER *hdr, uint8_t offset, uint16_t value) {
        volatile auto *ptr = reinterpret_cast<volatile uint16_t *>(reinterpret_cast<uint8_t *>(hdr) + offset);
        *ptr = value;
    }

    inline uint32_t pci_read32(pci::PCI_DEVICE_HEADER *hdr, uint8_t offset) {
        volatile auto *ptr = reinterpret_cast<volatile uint32_t *>(reinterpret_cast<uint8_t *>(hdr) + offset);
        return *ptr;
    }

    inline void pci_write32(pci::PCI_DEVICE_HEADER *hdr, uint8_t offset, uint32_t value) {
        volatile auto *ptr = reinterpret_cast<volatile uint32_t *>(reinterpret_cast<uint8_t *>(hdr) + offset);
        *ptr = value;
    }


    inline uint32_t pci_config_address(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
        return 1U << 31 // enable bit
               | (static_cast<uint32_t>(bus) << 16)
               | (static_cast<uint32_t>(device) << 11)
               | (static_cast<uint32_t>(function) << 8)
               | (offset & 0xFC);
    }


    void enumerate_pci(acpi::MCFG_HEADER *mcfg);

    extern const char *device_classes[];

    const char *get_vendor_name(uint16_t vendor_id);

    const char *get_device_name(uint16_t vendor_id, uint16_t device_id);

    const char *get_subclass_name(uint8_t class_code, uint8_t subclass_code);

    const char *get_prog_if_name(uint8_t class_code, uint8_t subclass_code, uint8_t prog_if);

    BarInfo get_bar_info(PCI_HEADER0 *header, uint8_t bar_index);
}

#endif //PCI_H
