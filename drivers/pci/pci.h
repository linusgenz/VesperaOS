//
// Created by linus on 06.10.24.
//

#ifndef PCI_H
#define PCI_H
#include <vespera/types.h>

#include <acpi/acpi.h>

namespace pci {
    struct PCI_DEVICE_HEADER {
        u16 vendor_id;
        u16 device_id;
        volatile u16 command;
        volatile u16 status;
        u8 revision_id;
        u8 prog_if;
        u8 subclass;
        u8 _class;
        u8 cache_line_size;
        u8 latency_timer;
        u8 header_type;
        u8 bist;
    };

    struct PCI_HEADER0 {
        PCI_DEVICE_HEADER header;
        u32 bar0;
        u32 bar1;
        u32 bar2;
        u32 bar3;
        u32 bar4;
        u32 bar5;
        u32 cardbus_cis_ptr;
        u16 subsystem_vendor_id;
        u16 subsystem_id;
        u32 expansion_robase_addr;
        u8 capabilities_ptr;
        u8 rsv0;
        u16 rsv1;
        u32 rsv2;
        u8 interrupt_line;
        u8 interrupt_pin;
        u8 min_grant;
        u8 max_latency;
    };

#define PCI_BAR_TYPE_32_BIT    0x0
#define PCI_BAR_TYPE_64_BIT    0x4
#define PCI_BAR_TYPE_MASK     0x6
#define PCI_BAR_MEMORY_MASK   0x1

    struct BarInfo {
        u64 address;
        u64 size;
        bool is_64_bit;
        bool is_memory;
        bool is_prefetchable;
        bool is_valid;
    };

    inline u8 pci_read8(pci::PCI_DEVICE_HEADER *hdr, u8 offset) {
        volatile u8 *ptr = reinterpret_cast<u8 *>(hdr) + offset;
        return *ptr;
    }

    inline void pci_write8(pci::PCI_DEVICE_HEADER *hdr, u8 offset, u8 value) {
        volatile u8 *ptr = reinterpret_cast<u8 *>(hdr) + offset;
        *ptr = value;
    }

    inline u16 pci_read16(pci::PCI_DEVICE_HEADER *hdr, u8 offset) {
        volatile auto *ptr = reinterpret_cast<volatile u16 *>(reinterpret_cast<u8 *>(hdr) + offset);
        return *ptr;
    }

    inline void pci_write16(pci::PCI_DEVICE_HEADER *hdr, u8 offset, u16 value) {
        volatile auto *ptr = reinterpret_cast<volatile u16 *>(reinterpret_cast<u8 *>(hdr) + offset);
        *ptr = value;
    }

    inline u32 pci_read32(pci::PCI_DEVICE_HEADER *hdr, u8 offset) {
        volatile auto *ptr = reinterpret_cast<volatile u32 *>(reinterpret_cast<u8 *>(hdr) + offset);
        return *ptr;
    }

    inline void pci_write32(pci::PCI_DEVICE_HEADER *hdr, u8 offset, u32 value) {
        volatile auto *ptr = reinterpret_cast<volatile u32 *>(reinterpret_cast<u8 *>(hdr) + offset);
        *ptr = value;
    }


    inline u32 pci_config_address(u8 bus, u8 device, u8 function, u8 offset) {
        return 1U << 31 // enable bit
               | (static_cast<u32>(bus) << 16)
               | (static_cast<u32>(device) << 11)
               | (static_cast<u32>(function) << 8)
               | (offset & 0xFC);
    }


    void enumerate_pci(acpi::MCFG_HEADER *mcfg);

    extern const char *device_classes[];

    const char *get_vendor_name(u16 vendor_id);

    const char *get_device_name(u16 vendor_id, u16 device_id);

    const char *get_subclass_name(u8 class_code, u8 subclass_code);

    const char *get_prog_if_name(u8 class_code, u8 subclass_code, u8 prog_if);

    BarInfo get_bar_info(PCI_HEADER0 *header, u8 bar_index);
}

#endif //PCI_H
