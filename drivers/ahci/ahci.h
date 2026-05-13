#ifndef AHCI_H
#define AHCI_H

#include <vespera/devices/block.h>
#include <vespera/devices/driver_lifecycle.h>
#include <vespera/interrupts.h>
#include <vespera/io/block_io_queue.h>
#include <vespera/sync/mutex.h>

#include "../pci/pci.h"
#include "ata.h"
#include "uapi/vespera/dev/ioctl_smart.h"
#include "vespera/devices/char_device.h"
#include "vespera/devices/device_info.h"
#include "vespera/devices/smart_device.h"

// https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/serial-ata-ahci-spec-rev1-3-1.pdf
namespace ahci {
#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08
#define ATA_CMD_READ_DMA_EX 0x25
#define ATA_CMD_WRITE_DMA_EX 0x35
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_FLUSH_CACHE 0xE7
#define ATA_CMD_FLUSH_CACHE_EXT 0xEA
#define ATA_CMD_SMART 0xB0
#define ATA_SMART_READ_DATA 0xD0
#define ATA_SMART_RETURN_STATUS 0xDA
#define ATA_CMD_DATA_SET_MANAGEMENT 0x06
#define ATA_DSM_TRIM 0x01

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
        u32 command_list_base;
        u32 command_list_base_upper;
        u32 fis_base_address;
        u32 fis_base_address_upper;
        u32 interrupt_status;
        u32 interrupt_enable;
        u32 cmd_sts;
        u32 rsv0;
        u32 task_file_data;
        u32 signature;
        u32 sata_status;
        u32 sata_control;
        u32 sata_error;
        u32 sata_active;
        u32 command_issue;
        u32 sata_notification;
        u32 fis_switch_control;
        u32 rsv1[11];
        u32 vendor[4];
    };

    struct HBA_MEMORY {
        u32 host_capability;
        u32 global_host_control;
        u32 interrupt_status;
        u32 ports_implemented;
        u32 version;
        u32 ccc_control;
        u32 ccc_ports;
        u32 enclosure_management_location;
        u32 enclosure_management_control;
        u32 host_capabilities_extended;
        u32 bios_handoff_ctrl_sts;
        u8 rsv0[0x74];
        u8 vendor[0x60];
        HBA_PORT ports[32];
    };

    struct HBA_COMMAND_HEADER {
        u8 command_fis_length : 5;
        u8 atapi : 1;
        u8 write : 1;
        u8 prefetchable : 1;

        u8 reset : 1;
        u8 bist : 1;
        u8 clear_busy : 1;
        u8 rsv0 : 1;
        u8 port_multiplier : 4;

        u16 prdt_length;
        u32 prdb_count;
        u32 command_table_base_address;
        u32 command_table_base_address_upper;
        u32 rsv1[4];
    };

    struct HBA_PRDT_ENTRY {
        u32 data_base_address;
        u32 data_base_address_upper;
        u32 rsv0;

        u32 byte_count : 22;
        u32 rsv1 : 9;
        u32 interrupt_on_completion : 1;
    };

    struct HbaCommandTable {
        u8 command_fis[64];

        u8 atapi_command[16];

        u8 rsv[48];

        HBA_PRDT_ENTRY prdt_entry[];
    };

    // ReSharper disable once CppInconsistentNaming
    struct FIS_REG_H2D {
        u8 fis_type;

        u8 port_multiplier : 4;
        u8 rsv0 : 3;
        u8 command_control : 1;

        u8 command;
        u8 feature_low;

        u8 lba0;
        u8 lba1;
        u8 lba2;
        u8 device_register;

        u8 lba3;
        u8 lba4;
        u8 lba5;
        u8 feature_high;

        u8 count_low;
        u8 count_high;
        u8 iso_command_completion;
        u8 control;

        u8 rsv1[4];
    };

    // ReSharper disable once CppInconsistentNaming
    struct FIS_REG_D2H {
        u8 fis_type;  // FIS_TYPE_REG_D2H

        u8 pmport : 4;     // Port multiplier
        u8 rsv0 : 2;       // Reserved
        u8 interrupt : 1;  // Interrupt bit
        u8 rsv1 : 1;       // Reserved

        u8 status;  // Status register
        u8 error;   // Error register

        u8 lba0;    // LBA low register, 7:0
        u8 lba1;    // LBA mid register, 15:8
        u8 lba2;    // LBA high register, 23:16
        u8 device;  // Device register

        u8 lba3;  // LBA register, 31:24
        u8 lba4;  // LBA register, 39:32
        u8 lba5;  // LBA register, 47:40
        u8 rsv2;  // Reserved

        u8 count_low;   // Count register, 7:0
        u8 count_high;  // Count register, 15:8
        u8 rsv3[2];     // Reserved

        uint8_t rsv4[4];  // Reserved
    };

    class Port final : public BlockDevice, public ISmartDevice, public IDeviceInfo {
       private:
        IDENTIFY_DEVICE_DATA* identify_ = nullptr;
        kernel::Mutex port_mutex_;
        bool smart_return_status();
        static void copy_ata_string(char* dst, usize dst_len, const u8* src, usize src_chars);

        bool has_flush_cache_ext_ = false;
        bool has_write_cache_ = false;
        bool has_smart_ = false;
        bool has_trim_ = false;

        BlockIoQueue io_queue_;

       public:
        u8 vector = 0;

        ~Port() override;

        HBA_PORT* hba_port{};
        PortType port_type;
        u8 port_number{};

        u32 sector_size = 0;
        u64 total_sectors = 0;

        volatile bool command_completed = false;
        volatile bool last_error = false;

        KernelDevice* kd;

        void interrupt_handler();
        void enable_interrupts() const;
        void configure() const;
        void stop_cmd() const;
        void start_cmd() const;

        bool flush();
        [[nodiscard]] bool write_cache_enabled() const;

        isize read(u64 lba, usize sector_count, void* buffer, usize buffer_size) override;
        isize write(u64 sector, usize sector_count, const void* buffer, usize buffer_size) override;

        isize do_write(u64 sector, usize sector_count, const void* buffer, usize buffer_size);
        isize do_read(u64 lba, usize sector_count, void* buffer, usize buffer_size);

        void start_io_worker(u8 cpu_id);
        void stop_io_worker();
        static void io_worker_entry(void* arg);

        [[nodiscard]] usize get_sector_size() const override;
        bool smart_read_data(u8* out_buf) override;
        bool smart_get_common(smart_common* out) override;
        bool smart_get_ata(smart_ata* out) override;
        bool trim(const TrimRange* ranges, usize count) override;
        [[nodiscard]] bool supports_trim() const override;
        bool get_model(char* out, usize len) override;
        bool get_serial(char* out, usize len) override;
        bool get_firmware(char* out, usize len) override;
        [[nodiscard]] usize get_size() const override;
        bool identify();
    };

    class AhciDriver final : public IDriverLifecycle, public IDeviceInfo {
       public:
        explicit AhciDriver(pci::PCI_DEVICE_HEADER* pci_base_address);
        bool get_vendor(char* out, usize len) override;
        bool get_model(char* out, usize len) override;
        ~AhciDriver() override;
        [[nodiscard]] bool has_active_ports() const;
        pci::PCI_DEVICE_HEADER* pci_base_address;
        HBA_MEMORY* abar;
        void probe_ports();
        static Irqreturn global_interrupt_handler(const AhciDriver* driver);
        Port* ports[32]{};
        u8 port_count;

        void on_shutdown() override;
        void on_suspend() override;

       private:
        KernelDevice* kd_;
    };
}  // namespace ahci

#endif  // AHCI_H
