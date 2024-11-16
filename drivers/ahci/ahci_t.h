//
// Created by linus on 13.10.24.
//

#ifndef AHCI_H
#define AHCI_H
#include <stdint.h>
#include "../pci/pci.h"

namespace AHCI {

    #define ATA_DEV_BUSY 0x80
    #define ATA_DEV_DRQ 0x08
    #define ATA_CMD_READ_DMA_EX 0x25

    #define HBA_PxIS_TFES (1 << 30)

    enum PortType {
        None = 0,
        SATA = 1,
        SEMB = 2,
        PM = 3,
        SATAPI = 4,
    };

    enum FIS_TYPE{
        FIS_TYPE_REG_H2D = 0x27,
        FIS_TYPE_REG_D2H = 0x34,
        FIS_TYPE_DMA_ACT = 0x39,
        FIS_TYPE_DMA_SETUP = 0x41,
        FIS_TYPE_DATA = 0x46,
        FIS_TYPE_BIST = 0x58,
        FIS_TYPE_PIO_SETUP = 0x5F,
        FIS_TYPE_DEV_BITS = 0xA1,
    };

    // Host bus adapter
    struct HBAPort{
        uint32_t command_list_base;
        uint32_t command_list_base_upper;
        uint32_t fis_base_address; // frame information structure
        uint32_t fis_base_address_upper;
        uint32_t interrupt_status;
        uint32_t interrupt_enable;
        uint32_t cmd_sts;
        uint32_t rsv0;
        uint32_t task_file_data;
        uint32_t signature;
        uint32_t sata_status;
        uint32_t sata_control;
        uint32_t sata_error;
        uint32_t sata_active;
        uint32_t command_issue;
        uint32_t sata_notification;
        uint32_t fis_switch_control;
        uint32_t rsv1[11];
        uint32_t vendor[4];
    };

    struct HBAMemory{
        uint32_t host_capability;
        uint32_t global_host_control;
        uint32_t interrupt_status;
        uint32_t ports_implemented;
        uint32_t version;
        uint32_t ccc_control;
        uint32_t ccc_ports;
        uint32_t enclosure_management_location;
        uint32_t enclosure_management_control;
        uint32_t host_capabilities_extended;
        uint32_t bios_handoff_ctrl_sts;
        uint8_t rsv0[0x74];
        uint8_t vendor[0x60];
        HBAPort ports[1];
    };

    struct HBACommandHeader {
        uint8_t command_fis_length:5;
        uint8_t atapi:1;
        uint8_t write:1;
        uint8_t prefetchable:1;

        uint8_t reset:1;
        uint8_t bist:1;
        uint8_t clear_busy:1;
        uint8_t rsv0:1;
        uint8_t port_multiplier:4;

        uint16_t prdt_length;
        uint32_t prdb_count;
        uint32_t command_table_base_address;
        uint32_t command_table_base_address_upper;
        uint32_t rsv1[4];
    };
    
    struct FIS_REG_H2D {
        uint8_t fis_type;

        uint8_t port_multiplier:4;
        uint8_t rsv0:3;
        uint8_t command_control:1;

        uint8_t command;
        uint8_t feature_low;

        uint8_t lba0; // linear base address
        uint8_t lba1;
        uint8_t lba2;
        uint8_t device_register;

        uint8_t lba3;
        uint8_t lba4;
        uint8_t lba5;
        uint8_t feature_high;

        uint8_t count_low;
        uint8_t count_high;
        uint8_t iso_command_completion;
        uint8_t control;

        uint8_t rsv1[4];
    };

        struct HBAPRDTEntry{
        uint32_t data_base_address;
        uint32_t data_base_address_upper;
        uint32_t rsv0;

        uint32_t byte_count:22;
        uint32_t rsv1:9;
        uint32_t interrupt_on_completion:1;
    };

    struct HBACommandTable{
        uint8_t command_fis[64];

        uint8_t atapi_command[16];

        uint8_t rsv[48];

        HBAPRDTEntry prdt_entry[];
    };

    class Port {
        public:
            HBAPort* hba_port;
            PortType port_type;
            uint8_t* buffer;
            uint8_t port_number;
            void configure();
            void start_cmd();
            void stop_cmd();
            bool read(uint64_t sector, uint32_t sector_count, void* buffer);
    };

    class AHCIDriver {
        public:
            AHCIDriver(PCI::PCIDeviceHeader* pci_base_address);
            ~AHCIDriver();
            PCI::PCIDeviceHeader* pci_base_address;
            HBAMemory* abar;
            void probe_ports();
            Port* ports[32];
            uint8_t port_count;
    };
}

#endif //AHCI_H