//
// Created by linus on 06.10.24.
//

#ifndef ACPI_H
#define ACPI_H
#include <vespera/types.h>

extern "C" {
#include "../kernel/acpi/acpica/include/acpi.h"
}

namespace acpi {

    struct RSDP {
        unsigned char signature[8];  // "RSD PTR "
        u8 checksum;
        u8 oem_id[6];
        u8 revision;  // 0 = ACPI 1.0, 2 = ACPI 2.0+
        u32 rsdt_address;
    } __attribute__((packed));

    struct RSDP2 {
        unsigned char signature[8];
        u8 checksum;
        u8 oem_id[6];
        u8 revision;
        u32 rsdt_address;
        // ACPI 2.0+ extension
        u32 length;
        u64 xsdt_address;
        u8 extended_checksum;
        u8 reserved[3];
    } __attribute__((packed));

    struct SDT_HEADER {
        unsigned char signature[4];
        u32 length;
        u8 revision;
        u8 checksum;
        u8 oem_id[6];
        u8 oem_table_id[8];
        u32 oem_revision;
        u32 creator_id;
        u32 creator_revision;
    } __attribute__((packed));

    struct RSDT {
        SDT_HEADER header;
        u32 entries[];
    } __attribute__((packed));

    struct XSDT {
        SDT_HEADER header;
        u64 entries[];
    } __attribute__((packed));

    struct MCFG_HEADER {
        SDT_HEADER header;
        u64 reserved;
    } __attribute__((packed));

    struct DeviceConfig {
        u64 base_address;
        u16 pci_seg_group;
        u8 start_bus;
        u8 end_bus;
        u32 reserved;
    } __attribute__((packed));

    struct MADT_HEADER {
        SDT_HEADER header;  // ACPI Standard Header (signature = "APIC")
        u32 lapic_address;
        u32 flags;  // Bit 0 = PCAT_COMPAT (Legacy PICs installed)
    } __attribute__((packed));

    enum class MADT_ENTRY_TYPE : u8 {
        LOCAL_APIC = 0,
        IO_APIC = 1,
        INTERRUPT_OVERRIDE = 2,
        NMI_SOURCE = 3,
        LOCAL_APIC_NMI = 4,
        LOCAL_APIC_OVERRIDE = 5,
        X2_APIC = 9
    };

    struct MADT_ENTRY_HEADER {
        u8 type;
        u8 length;
    } __attribute__((packed));

    struct LOCAL_APIC_ENTRY {
        MADT_ENTRY_HEADER header;
        u8 acpi_processor_id;
        u8 apic_id;
        u32 flags;
    } __attribute__((packed));

    struct IOAPIC_ENTRY {
        MADT_ENTRY_HEADER header;
        u8 ioapic_id;
        u8 reserved;
        u32 ioapic_address;
        u32 gsi_base;
    } __attribute__((packed));

    struct InterruptOverrideEntry {
        MADT_ENTRY_HEADER header;
        u8 bus;
        u8 irq_source;
        u32 gsi;
        u16 flags;
    } __attribute__((packed));

    struct LAPICNMI_ENTRY {
        MADT_ENTRY_HEADER header;
        u8 acpi_processor_id;
        u16 flags;
        u8 lint;
    } __attribute__((packed));

    struct LAPIC_OVERRIDE_ENTRY {
        MADT_ENTRY_HEADER header;
        u16 reserved;
        u64 lapic_address;
    } __attribute__((packed));

    struct X2_APIC_ENTRY {
        MADT_ENTRY_HEADER header;
        u16 reserved;
        u32 x2_apic_id;
        u32 flags;
        u32 acpi_id;
    } __attribute__((packed));

    struct GENERIC_ADDRESS_STRUCTURE {
        u8 address_space;
        u8 bit_width;
        u8 bit_offset;
        u8 access_size;
        u64 address;
    } __attribute__((packed));

    struct FADT {
        SDT_HEADER header;
        u32 firmware_ctrl;
        u32 dsdt;

        u8 reserved;

        u8 preferred_pm_profile;
        u16 sci_interrupt;
        u32 smi_command_port;
        u8 acpi_enable;
        u8 acpi_disable;
        u8 s4_bios_req;
        u8 pstate_control;

        u32 pm1_a_event_block;
        u32 pm1_b_event_block;
        u32 pm1_a_control_block;
        u32 pm1_b_control_block;
        u32 pm2_control_block;
        u32 pm_timer_block;
        u32 gpe0_block;
        u32 gpe1_block;

        u8 pm1_event_length;
        u8 pm1_control_length;
        u8 pm2_control_length;
        u8 pm_timer_length;
        u8 gpe0_length;
        u8 gpe1_length;
        u8 gpe1_base;

        u8 cstate_control;
        u16 worst_c2_latency;
        u16 worst_c3_latency;
        u16 flush_size;
        u16 flush_stride;
        u8 duty_offset;
        u8 duty_width;
        u8 day_alarm;
        u8 month_alarm;
        u8 century;

        u16 boot_architecture_flags;

        u8 reserved2;
        u32 flags;

        GENERIC_ADDRESS_STRUCTURE reset_reg;
        u8 reset_value;
        u8 reserved3[3];

        u64 x_firmware_ctrl;
        u64 x_dsdt;

        GENERIC_ADDRESS_STRUCTURE x_pm1_a_event_block;
        GENERIC_ADDRESS_STRUCTURE x_pm1_b_event_block;
        GENERIC_ADDRESS_STRUCTURE x_pm1_a_control_block;
        GENERIC_ADDRESS_STRUCTURE x_pm1_b_control_block;
        GENERIC_ADDRESS_STRUCTURE x_pm2_control_block;
        GENERIC_ADDRESS_STRUCTURE x_pm_timer_block;
        GENERIC_ADDRESS_STRUCTURE x_gpe0_block;
        GENERIC_ADDRESS_STRUCTURE x_gpe1_block;
    } __attribute__((packed));

    void acpi_reboot();
    void acpi_power_off();
}  // namespace acpi

#endif  // ACPI_H