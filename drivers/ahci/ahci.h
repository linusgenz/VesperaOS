#ifndef AHCI_H
#define AHCI_H

#include "../../arch/x86_64/interrupts/idt.h"
#include "../../filesystem/devfs/devfs.h"
#include "../../include/kernel/sync/mutex.h"
#include "../../kernel/devices/blockdevice.h"
#include "../pci/pci.h"
#include "ata.h"
#include "kernel/devices/device_manager.h"

// https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/serial-ata-ahci-spec-rev1-3-1.pdf
namespace ahci {
#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08
#define ATA_CMD_READ_DMA_EX 0x25
#define ATA_CMD_WRITE_DMA_EX 0x35
#define ATA_CMD_IDENTIFY 0xEC

#define HBA_PX_IS_TFES (1 << 30)

    // Bit 31 — AE (AHCI Enable)
#define AHCI_GHC_AE (1u << 31)

    // Bit 2  — MRSM (MSI Revert to Single Message) — read-only
#define AHCI_GHC_MRSM (1u << 2)

    // Bit 1  — IE (Interrupt Enable)
#define AHCI_GHC_IE (1u << 1)

    // Bit 0  — HR (HBA Reset)
#define AHCI_GHC_HR (1u << 0)

    enum PortType {
        None = 0,
        Sata = 1,
        Semb = 2,
        Pm = 3,
        Satapi = 4,
    };

    enum FIS_TYPE {
        // ReSharper disable once CppInconsistentNaming
        FIS_TYPE_REG_H2D = 0x27,
        // ReSharper disable once CppInconsistentNaming
        FIS_TYPE_REG_D2H = 0x34,
        FIS_TYPE_DMA_ACT = 0x39,
        FIS_TYPE_DMA_SETUP = 0x41,
        FIS_TYPE_DATA = 0x46,
        FIS_TYPE_BIST = 0x58,
        FIS_TYPE_PIO_SETUP = 0x5F,
        FIS_TYPE_DEV_BITS = 0xA1,
    };

    struct HBA_PORT {
        uint32_t command_list_base;
        uint32_t command_list_base_upper;
        uint32_t fis_base_address;
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

    struct HBA_MEMORY {
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
        HBA_PORT ports[32];
    };

    struct HBA_COMMAND_HEADER {
        uint8_t command_fis_length : 5;
        uint8_t atapi : 1;
        uint8_t write : 1;
        uint8_t prefetchable : 1;

        uint8_t reset : 1;
        uint8_t bist : 1;
        uint8_t clear_busy : 1;
        uint8_t rsv0 : 1;
        uint8_t port_multiplier : 4;

        uint16_t prdt_length;
        uint32_t prdb_count;
        uint32_t command_table_base_address;
        uint32_t command_table_base_address_upper;
        uint32_t rsv1[4];
    };

    struct HBA_PRDT_ENTRY {
        uint32_t data_base_address;
        uint32_t data_base_address_upper;
        uint32_t rsv0;

        uint32_t byte_count : 22;
        uint32_t rsv1 : 9;
        uint32_t interrupt_on_completion : 1;
    };

    struct HbaCommandTable {
        uint8_t command_fis[64];

        uint8_t atapi_command[16];

        uint8_t rsv[48];

        HBA_PRDT_ENTRY prdt_entry[];
    };

    struct FisRegH2D {
        uint8_t fis_type;

        uint8_t port_multiplier : 4;
        uint8_t rsv0 : 3;
        uint8_t command_control : 1;

        uint8_t command;
        uint8_t feature_low;

        uint8_t lba0;
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

    class Port final : public BlockDevice {
       private:
        IDENTIFY_DEVICE_DATA* identify_ = nullptr;
        kernel::Mutex port_mutex_;

       public:
        uint8_t vector = 0;

        ~Port() override;

        HBA_PORT* hba_port{};
        PortType port_type;
        uint8_t port_number{};

        uint32_t sector_size = 0;
        uint64_t total_sectors = 0;

        volatile bool command_completed = false;
        volatile bool last_error = false;

        KernelDevice* kd;

        void interrupt_handler();
        void enable_interrupts() const;
        void configure() const;
        void stop_cmd() const;
        void start_cmd() const;

        ssize_t read(uint64_t lba, size_t sector_count, void* buffer, size_t buffer_size) override;

        ssize_t write(uint64_t sector, size_t sector_count, void* buffer, size_t buffer_size) override;

        [[nodiscard]] size_t get_sector_size() const override;
        [[nodiscard]] size_t get_size() const override;
        bool identify();
    };

    class AhciDriver {
       public:
        explicit AhciDriver(pci::PCI_DEVICE_HEADER* pci_base_address);
        ~AhciDriver();
        [[nodiscard]] bool has_active_ports() const;
        pci::PCI_DEVICE_HEADER* pci_base_address;
        HBA_MEMORY* abar;
        void probe_ports();
        static Irqreturn global_interrupt_handler(const AhciDriver* driver);
        Port* ports[32]{};
        uint8_t port_count;

       private:
        KernelDevice* kd_;
    };
}  // namespace ahci

#endif  // AHCI_H
