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
namespace AHCI {
#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08
#define ATA_CMD_READ_DMA_EX 0x25
#define ATA_CMD_WRITE_DMA_EX 0x35
#define ATA_CMD_IDENTIFY 0xEC

#define HBA_PxIS_TFES (1 << 30)

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
        SATA = 1,
        SEMB = 2,
        PM = 3,
        SATAPI = 4,
    };

    enum FIS_TYPE {
        FIS_TYPE_REG_H2D = 0x27,
        FIS_TYPE_REG_D2H = 0x34,
        FIS_TYPE_DMA_ACT = 0x39,
        FIS_TYPE_DMA_SETUP = 0x41,
        FIS_TYPE_DATA = 0x46,
        FIS_TYPE_BIST = 0x58,
        FIS_TYPE_PIO_SETUP = 0x5F,
        FIS_TYPE_DEV_BITS = 0xA1,
    };

    struct HBAPort {
        uint32_t commandListBase;
        uint32_t commandListBaseUpper;
        uint32_t fisBaseAddress;
        uint32_t fisBaseAddressUpper;
        uint32_t interruptStatus;
        uint32_t interruptEnable;
        uint32_t cmdSts;
        uint32_t rsv0;
        uint32_t taskFileData;
        uint32_t signature;
        uint32_t sataStatus;
        uint32_t sataControl;
        uint32_t sataError;
        uint32_t sataActive;
        uint32_t commandIssue;
        uint32_t sataNotification;
        uint32_t fisSwitchControl;
        uint32_t rsv1[11];
        uint32_t vendor[4];
    };

    struct HBAMemory {
        uint32_t hostCapability;
        uint32_t globalHostControl;
        uint32_t interruptStatus;
        uint32_t portsImplemented;
        uint32_t version;
        uint32_t cccControl;
        uint32_t cccPorts;
        uint32_t enclosureManagementLocation;
        uint32_t enclosureManagementControl;
        uint32_t hostCapabilitiesExtended;
        uint32_t biosHandoffCtrlSts;
        uint8_t rsv0[0x74];
        uint8_t vendor[0x60];
        HBAPort ports[32];
    };

    struct HBACommandHeader {
        uint8_t commandFISLength : 5;
        uint8_t atapi : 1;
        uint8_t write : 1;
        uint8_t prefetchable : 1;

        uint8_t reset : 1;
        uint8_t bist : 1;
        uint8_t clearBusy : 1;
        uint8_t rsv0 : 1;
        uint8_t portMultiplier : 4;

        uint16_t prdtLength;
        uint32_t prdbCount;
        uint32_t commandTableBaseAddress;
        uint32_t commandTableBaseAddressUpper;
        uint32_t rsv1[4];
    };

    struct HBAPRDTEntry {
        uint32_t dataBaseAddress;
        uint32_t dataBaseAddressUpper;
        uint32_t rsv0;

        uint32_t byteCount : 22;
        uint32_t rsv1 : 9;
        uint32_t interruptOnCompletion : 1;
    };

    struct HBACommandTable {
        uint8_t commandFIS[64];

        uint8_t atapiCommand[16];

        uint8_t rsv[48];

        HBAPRDTEntry prdtEntry[];
    };

    struct FIS_REG_H2D {
        uint8_t fisType;

        uint8_t portMultiplier : 4;
        uint8_t rsv0 : 3;
        uint8_t commandControl : 1;

        uint8_t command;
        uint8_t featureLow;

        uint8_t lba0;
        uint8_t lba1;
        uint8_t lba2;
        uint8_t deviceRegister;

        uint8_t lba3;
        uint8_t lba4;
        uint8_t lba5;
        uint8_t featureHigh;

        uint8_t countLow;
        uint8_t countHigh;
        uint8_t isoCommandCompletion;
        uint8_t control;

        uint8_t rsv1[4];
    };

    class Port final : public BlockDevice {
       private:
        IDENTIFY_DEVICE_DATA* identify = nullptr;
        kernel::mutex_t portMutex;

       public:
        uint8_t vector = 0;

        ~Port() override;

        HBAPort* hbaPort{};
        PortType portType;
        uint8_t portNumber{};

        uint32_t sector_size = 0;
        uint64_t total_sectors = 0;

        volatile bool commandCompleted = false;
        volatile bool lastError = false;

        KernelDevice* kd;

        void InterruptHandler();
        void EnableInterrupts() const;
        void Configure() const;
        void StopCMD() const;
        void StartCMD() const;

        ssize_t read(uint64_t lba, size_t sectorCount, void* buffer, size_t bufferSize) override;

        ssize_t write(uint64_t sector, size_t sectorCount, void* buffer, size_t bufferSize) override;

        [[nodiscard]] size_t get_sector_size() const override;
        [[nodiscard]] size_t get_size() const override;
        bool Identify();
    };

    class AHCIDriver {
       public:
        explicit AHCIDriver(PCI::PCIDeviceHeader* pciBaseAddress);
        ~AHCIDriver();
        [[nodiscard]] bool HasActivePorts() const;
        PCI::PCIDeviceHeader* PCIBaseAddress;
        HBAMemory* ABAR;
        void ProbePorts();
        static irqreturn_t GlobalInterruptHandler(const AHCIDriver* driver);
        Port* ports[32]{};
        uint8_t portCount;

       private:
        KernelDevice* kd;
    };
}  // namespace AHCI

#endif  // AHCI_H
