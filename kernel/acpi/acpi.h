//
// Created by linus on 06.10.24.
//

#ifndef ACPI_H
#define ACPI_H
#include <cstdint>

namespace ACPI {
    struct RSDP2 {
        unsigned char signature[8];
        uint8_t checksum;
        uint8_t oem_id[6];
        uint8_t revision;
        uint32_t rsdt_address;
        uint32_t length;
        uint64_t xsdt_address;
        uint8_t extended_checksum;
        uint8_t reserved[3];
    } __attribute__((packed));

    struct SDTHeader {
        unsigned char signature[4];
        uint32_t length;
        uint8_t revision;
        uint8_t checksum;
        uint8_t oem_id[6];
        uint8_t oem_table_id[8];
        uint32_t oem_revision;
        uint32_t creator_id;
        uint32_t creator_revision;
    }__attribute__((packed));

    struct MCFGHeader {
        SDTHeader header;
        uint64_t reserved;
    }__attribute__((packed));

    struct DeviceConfig {
        uint64_t base_address;
        uint16_t pci_seg_group;
        uint8_t start_bus;
        uint8_t end_bus;
        uint32_t reserved;
    }__attribute__((packed));


    struct MADTHeader {
        SDTHeader header;      // ACPI Standard Header (signature = "APIC")
        uint32_t lapic_address;
        uint32_t flags;        // Bit 0 = PCAT_COMPAT (Legacy PICs installed)
    } __attribute__((packed));

    enum class MADTEntryType : uint8_t {
        LOCAL_APIC            = 0,
        IO_APIC               = 1,
        INTERRUPT_OVERRIDE    = 2,
        NMI_SOURCE            = 3,
        LOCAL_APIC_NMI        = 4,
        LOCAL_APIC_OVERRIDE   = 5,
        X2APIC                = 9
    };

    struct MADTEntryHeader {
        uint8_t type;
        uint8_t length;
    } __attribute__((packed));

    struct LocalAPICEntry {
        MADTEntryHeader header;
        uint8_t acpi_processor_id;
        uint8_t apic_id;
        uint32_t flags;
    } __attribute__((packed));

    struct IOAPICEntry {
        MADTEntryHeader header;
        uint8_t ioapic_id;
        uint8_t reserved;
        uint32_t ioapic_address;
        uint32_t gsi_base;
    } __attribute__((packed));

    struct InterruptOverrideEntry {
        MADTEntryHeader header;
        uint8_t bus;
        uint8_t irq_source;
        uint32_t gsi;
        uint16_t flags;
    } __attribute__((packed));

    struct LAPICNMIEntry {
        MADTEntryHeader header;
        uint8_t acpi_processor_id;
        uint16_t flags;
        uint8_t lint;
    } __attribute__((packed));

    struct LAPICOverrideEntry {
        MADTEntryHeader header;
        uint16_t reserved;
        uint64_t lapic_address;
    } __attribute__((packed));

    struct X2APICEntry {
        MADTEntryHeader header;
        uint16_t reserved;
        uint32_t x2apic_id;
        uint32_t flags;
        uint32_t acpi_id;
    } __attribute__((packed));


    struct GenericAddressStructure
    {
        uint8_t AddressSpace;
        uint8_t BitWidth;
        uint8_t BitOffset;
        uint8_t AccessSize;
        uint64_t Address;
    };

    struct generic_address_structure {
        uint8_t address_space;
        uint8_t bit_width;
        uint8_t bit_offset;
        uint8_t access_size;
        uint64_t address;
    } __attribute__((packed));

    struct FADT {
        SDTHeader header;
        uint32_t firmware_ctrl;
        uint32_t dsdt;

        uint8_t reserved;

        uint8_t preferred_pm_profile;
        uint16_t sci_interrupt;
        uint32_t smi_command_port;
        uint8_t acpi_enable;
        uint8_t acpi_disable;
        uint8_t s4bios_req;
        uint8_t pstate_control;

        uint32_t pm1a_event_block;
        uint32_t pm1b_event_block;
        uint32_t pm1a_control_block;
        uint32_t pm1b_control_block;
        uint32_t pm2_control_block;
        uint32_t pm_timer_block;
        uint32_t gpe0_block;
        uint32_t gpe1_block;

        uint8_t pm1_event_length;
        uint8_t pm1_control_length;
        uint8_t pm2_control_length;
        uint8_t pm_timer_length;
        uint8_t gpe0_length;
        uint8_t gpe1_length;
        uint8_t gpe1_base;

        uint8_t cstate_control;
        uint16_t worst_c2_latency;
        uint16_t worst_c3_latency;
        uint16_t flush_size;
        uint16_t flush_stride;
        uint8_t duty_offset;
        uint8_t duty_width;
        uint8_t day_alarm;
        uint8_t month_alarm;
        uint8_t century;

        uint16_t boot_architecture_flags;

        uint8_t reserved2;
        uint32_t flags;

        generic_address_structure reset_reg;
        uint8_t reset_value;
        uint8_t reserved3[3];

        uint64_t x_firmware_ctrl;
        uint64_t x_dsdt;

        generic_address_structure x_pm1a_event_block;
        generic_address_structure x_pm1b_event_block;
        generic_address_structure x_pm1a_control_block;
        generic_address_structure x_pm1b_control_block;
        generic_address_structure x_pm2_control_block;
        generic_address_structure x_pm_timer_block;
        generic_address_structure x_gpe0_block;
        generic_address_structure x_gpe1_block;
    } __attribute__((packed));

    void* find_table(SDTHeader* sdt_header, char* signature);

    void acpi_reboot();
    void acpi_power_off();
    void parse_fadt();
}

#endif //ACPI_H