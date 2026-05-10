//
// Created by linus on 06.10.24.
//

#ifndef PCI_H
#define PCI_H
#include <acpi/acpi.h>
#include <vespera/types.h>

namespace kernel::acpi {
    struct MCFG_HEADER;
    struct DEVICE_CONFIG;
}  // namespace kernel::acpi

namespace pci {

    /**
     * @brief Common PCI device/function header (first 16 bytes of config space).
     *
     * Shared by all header types (0, 1, 2). Mapped directly from MMIO config space.
     */
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

    /**
     * @brief PCI Type-0 (endpoint) configuration space header.
     *
     * Covers the full 64-byte Type-0 layout including all six BARs and the
     * capabilities pointer. Mapped directly from MMIO config space via MCFG.
     *
     * @note The capabilities pointer is valid only when bit 4 of @c status is set.
     */

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

    constexpr u32 PCI_BAR_MEMORY_MASK = 0x1u;  ///< Bit 0: 0 = memory BAR, 1 = I/O BAR.
    constexpr u32 PCI_BAR_TYPE_MASK = 0x6u;    ///< Bits [2:1]: memory BAR type field.
    constexpr u32 PCI_BAR_TYPE_32_BIT = 0x0u;  ///< Bits [2:1] == 0b00 — 32-bit memory BAR.
    constexpr u32 PCI_BAR_TYPE_64_BIT = 0x4u;  ///< Bits [2:1] == 0b10 — 64-bit memory BAR.

    /**
     * @brief Decoded BAR descriptor returned by @ref pci::bar::read.
     */
    struct BarInfo {
        u64 address;           ///< Base address (physical, already masked).
        u64 size;              ///< BAR aperture size in bytes.
        bool is_64_bit;        ///< True if this is a 64-bit BAR (spans two config-space slots).
        bool is_memory;        ///< True for memory BARs, false for I/O BARs.
        bool is_prefetchable;  ///< True if the memory range is marked prefetchable.
        bool is_valid;         ///< False when the BAR is absent or the index is out of range.
    };

    inline u8 pci_read8(PCI_DEVICE_HEADER* hdr, u8 offset) {
        return *reinterpret_cast<volatile u8*>(reinterpret_cast<u8*>(hdr) + offset);
    }

    inline void pci_write8(PCI_DEVICE_HEADER* hdr, u8 offset, u8 value) {
        *reinterpret_cast<volatile u8*>(reinterpret_cast<u8*>(hdr) + offset) = value;
    }

    inline u16 pci_read16(PCI_DEVICE_HEADER* hdr, u8 offset) {
        return *reinterpret_cast<volatile u16*>(reinterpret_cast<u8*>(hdr) + offset);
    }

    inline void pci_write16(PCI_DEVICE_HEADER* hdr, u8 offset, u16 value) {
        *reinterpret_cast<volatile u16*>(reinterpret_cast<u8*>(hdr) + offset) = value;
    }

    inline u32 pci_read32(PCI_DEVICE_HEADER* hdr, u8 offset) {
        return *reinterpret_cast<volatile u32*>(reinterpret_cast<u8*>(hdr) + offset);
    }

    inline void pci_write32(PCI_DEVICE_HEADER* hdr, u8 offset, u32 value) {
        *reinterpret_cast<volatile u32*>(reinterpret_cast<u8*>(hdr) + offset) = value;
    }

    /**
     * @brief Builds a CF8 configuration address for legacy I/O-port PCI access.
     *
     * The returned value is suitable for writing to I/O port 0xCF8. Bit 31 is
     * the enable bit; bus/device/function/offset are placed per the PCI spec.
     *
     * @note This is only needed for legacy I/O-port access. MMIO (MCFG) access
     *       simply indexes into the mapped config space directly.
     */

    inline u32 pci_config_address(u8 bus, u8 device, u8 function, u8 offset) {
        return 1U << 31  // enable bit
               | (static_cast<u32>(bus) << 16) | (static_cast<u32>(device) << 11) | (static_cast<u32>(function) << 8) |
               (offset & 0xFC);
    }

    /**
     * @brief Enumerates all PCI devices described by the ACPI MCFG table.
     *
     * Iterates every MCFG segment, walks buses, devices, and functions, maps
     * each config space page, and calls @ref driver_registry::bind for every
     * present function.
     *
     * @param mcfg  Pointer to the MCFG table header, as returned by ACPICA.
     *
     * @note Must be called after the MMIO address space is available and the
     *       driver registry has been populated via @ref driver_registry::init_drivers.
     */

    void enumerate_pci(kernel::acpi::MCFG_HEADER* mcfg);

    const char* get_vendor_name(u16 vendor_id);
    const char* get_device_name(u16 vendor_id, u16 device_id);
    const char* get_subclass_name(u8 class_code, u8 subclass_code);
    const char* get_prog_if_name(u8 class_code, u8 subclass_code, u8 prog_if);
}  // namespace pci

#endif  // PCI_H
