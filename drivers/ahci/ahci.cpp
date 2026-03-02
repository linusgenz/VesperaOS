#include "ahci.h"

#include <kernel/memory.h>

#include "../../filesystem/devfs/devfs.h"
#include "../../include/log.h"
#include "../../kernel/types/handle.h"
#include "../../userspace/lib/include/errno.h"
#include "../pci/msi.h"
#include "../pci/msix.h"
#include "ata.h"
#include "kernel/devices/device_manager.h"
#include "kernel/interrupts.h"

namespace AHCI {
#define HBA_PORT_DEV_PRESENT 0x3
#define HBA_PORT_IPM_ACTIVE 0x1
#define SATA_SIG_ATAPI 0xEB140101
#define SATA_SIG_ATA 0x00000101
#define SATA_SIG_SEMB 0xC33C0101
#define SATA_SIG_PM 0x96690101

#define HBA_PxCMD_CR 0x8000
#define HBA_PxCMD_FRE 0x0010
#define HBA_PxCMD_ST 0x0001
#define HBA_PxCMD_FR 0x4000

    PortType CheckPortType(const HBAPort* port) {
        uint32_t sataStatus = port->sataStatus;

        const uint8_t interfacePowerManagement = sataStatus >> 8 & 0b111;

        if (const uint8_t deviceDetection = sataStatus & 0b111; deviceDetection != HBA_PORT_DEV_PRESENT) return None;
        if (interfacePowerManagement != HBA_PORT_IPM_ACTIVE) return None;

        switch (port->signature) {
            case SATA_SIG_ATAPI:
                return SATAPI;
            case SATA_SIG_ATA:
                return SATA;
            case SATA_SIG_PM:
                return PM;
            case SATA_SIG_SEMB:
                return SEMB;
            default:
                return None;
        }
    }

    void AHCIDriver::ProbePorts() {
        for (int i = 0; i < 32; i++) {
            if (!(ABAR->portsImplemented & (1 << i))) continue;

            PortType portType = CheckPortType(&ABAR->ports[i]);
            if (portType != SATA && portType != SATAPI) continue;

            // we only want ports which have a device present with Phy communication established
            if (const uint32_t ssts = ABAR->ports[i].sataStatus; (ssts & 0xF) != 3) continue;

            ports[portCount] = new Port();
            ports[portCount]->portType = portType;
            ports[portCount]->hbaPort = &ABAR->ports[i];
            ports[portCount]->portNumber = portCount;
            portCount++;
        }
    }

    void Port::InterruptHandler() {
        uint32_t is = hbaPort->interruptStatus;

        if (!is) return;  // no Interrupt

        if (is & HBA_PxIS_TFES) {
            lastError = true;
            Log::Error("[AHCI] Port %u transfer error", portNumber);
        }

        commandCompleted = true;

        hbaPort->interruptStatus = is;
    }

    irqreturn_t AHCIDriver::GlobalInterruptHandler(const AHCIDriver* driver) {
        const uint32_t is = driver->ABAR->interruptStatus;

        if (!is) return IRQ_NONE;  // no Interrupt

        for (int i = 0; i < driver->portCount; i++) {
            if (is & (1 << driver->ports[i]->portNumber)) {
                driver->ports[i]->InterruptHandler();
            }
        }

        driver->ABAR->interruptStatus = is;

        return IRQ_HANDLED;
    }

    void Port::EnableInterrupts() const {
        hbaPort->interruptStatus = 0xFFFFFFFF;
        hbaPort->interruptEnable = 0xFFFFFFFF;
    }

    void Port::Configure() const {
        StopCMD();

        uint64_t cmdListPhys = kernel::memory::request_page_phys();
        void* cmdListVirt = phys_to_virt(cmdListPhys);

        hbaPort->commandListBase = static_cast<uint32_t>(cmdListPhys);
        hbaPort->commandListBaseUpper = static_cast<uint32_t>(cmdListPhys >> 32);

        auto* cmdHeader = static_cast<HBACommandHeader*>(cmdListVirt);
        memset(cmdListVirt, 0, 1024);

        uint64_t fisPhys = kernel::memory::request_page_phys();
        void* fisVirt = phys_to_virt(fisPhys);

        hbaPort->fisBaseAddress = static_cast<uint32_t>(fisPhys);
        hbaPort->fisBaseAddressUpper = static_cast<uint32_t>(fisPhys >> 32);
        memset(fisVirt, 0, 256);

        for (int i = 0; i < 32; i++) {
            cmdHeader[i].prdtLength = 8;

            uint64_t tablePhys = kernel::memory::request_page_phys();
            void* tableVirt = phys_to_virt(tablePhys);

            uint64_t tablePhysOffset = tablePhys + (i << 8);
            cmdHeader[i].commandTableBaseAddress = static_cast<uint32_t>(tablePhysOffset);
            cmdHeader[i].commandTableBaseAddressUpper = static_cast<uint32_t>(tablePhysOffset >> 32);
            memset(tableVirt, 0, 256);
        }

        StartCMD();
    }

    void Port::StopCMD() const {
        hbaPort->cmdSts &= ~HBA_PxCMD_ST;
        hbaPort->cmdSts &= ~HBA_PxCMD_FRE;

        while (true) {
            if (hbaPort->cmdSts & HBA_PxCMD_FR) continue;
            if (hbaPort->cmdSts & HBA_PxCMD_CR) continue;

            break;
        }
    }

    void Port::StartCMD() const {
        while (hbaPort->cmdSts & HBA_PxCMD_CR) {
        }

        hbaPort->cmdSts |= HBA_PxCMD_FRE;
        hbaPort->cmdSts |= HBA_PxCMD_ST;
    }

    size_t Port::get_size() const {
        return total_sectors * sector_size;
    }

    uint32_t Port::get_sector_size() const {
        return sector_size;
    }

    bool Port::Identify() {
        uint64_t identifyPhys = kernel::memory::request_page_phys();
        identify = static_cast<IDENTIFY_DEVICE_DATA*>(phys_to_virt(identifyPhys));
        memset(identify, 0, 0x1000);

        kernel::mutex_guard guard(portMutex);
        hbaPort->interruptStatus = static_cast<uint32_t>(-1);

        uint64_t cmdListPhys = static_cast<uint64_t>(hbaPort->commandListBaseUpper) << 32 |
                               static_cast<uint64_t>(hbaPort->commandListBase);
        auto* cmdHeader = static_cast<HBACommandHeader*>(phys_to_virt(cmdListPhys));

        cmdHeader->commandFISLength = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
        cmdHeader->write = 0;
        cmdHeader->prdtLength = 1;

        const uint64_t cmdTablePhys = static_cast<uint64_t>(cmdHeader->commandTableBaseAddressUpper) << 32 |
                                      static_cast<uint64_t>(cmdHeader->commandTableBaseAddress);
        auto* commandTable = static_cast<HBACommandTable*>(phys_to_virt(cmdTablePhys));
        memset(commandTable, 0, sizeof(HBACommandTable) + (cmdHeader->prdtLength - 1) * sizeof(HBAPRDTEntry));

        commandTable->prdtEntry[0].dataBaseAddress = static_cast<uint32_t>(identifyPhys);
        commandTable->prdtEntry[0].dataBaseAddressUpper = static_cast<uint32_t>(identifyPhys >> 32);
        commandTable->prdtEntry[0].byteCount = 511;
        commandTable->prdtEntry[0].interruptOnCompletion = 1;

        auto* cmdFIS = reinterpret_cast<FIS_REG_H2D*>(&commandTable->commandFIS);
        cmdFIS->fisType = FIS_TYPE_REG_H2D;
        cmdFIS->commandControl = 1;
        cmdFIS->command = ATA_CMD_IDENTIFY;

        hbaPort->commandIssue = 1;

        while (true) {
            if ((hbaPort->commandIssue & 1) == 0) break;
            if (hbaPort->interruptStatus & HBA_PxIS_TFES) {
                Log::Error("[ AHCI ] IDENTIFY error");
                return false;
            }
        }

        // Set sector_size

        if (identify->PhysicalLogicalSectorSize.LogicalSectorLongerThan256Words) {
            uint32_t wordsPerSector =
                identify->WordsPerLogicalSector[0] | (static_cast<uint32_t>(identify->WordsPerLogicalSector[1]) << 16);
            sector_size = wordsPerSector * 2;  // Words -> Bytes
        } else {
            sector_size = 512;  // Standard 512 Bytes
        }

        // Set total_sectors

        if (identify->AdditionalSupported.ExtendedUserAddressableSectorsSupported) {
            total_sectors = static_cast<uint64_t>(identify->ExtendedNumberOfUserAddressableSectors[1]) << 32 |
                            identify->ExtendedNumberOfUserAddressableSectors[0];
        } else {
            total_sectors = identify->UserAddressableSectors;
        }

        return true;
    }

    ssize_t Port::read(const uint64_t sector, const uint32_t sectorCount, void* buffer, size_t bufferSize) {
        size_t bytes = static_cast<size_t>(sectorCount) * sector_size;
        if (!buffer || sectorCount == 0 || bufferSize < bytes) return -EINVAL;
        kernel::mutex_guard guard(portMutex);

        size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        uint64_t dma_phys = kernel::memory::request_pages_phys(pages);
        void* dma = phys_to_virt(dma_phys);
        if (!dma_phys) return -ENOMEM;

        const auto sectorL = static_cast<uint32_t>(sector);
        const auto sectorH = static_cast<uint32_t>(sector >> 32);

        hbaPort->interruptStatus = 0xFFFFFFFF;  // Clear pending interrupt bits

        const uint64_t cmdListPhys = static_cast<uint64_t>(hbaPort->commandListBaseUpper) << 32 |
                                     static_cast<uint64_t>(hbaPort->commandListBase);
        auto* cmdHeader = static_cast<HBACommandHeader*>(phys_to_virt(cmdListPhys));
        cmdHeader->commandFISLength = sizeof(FIS_REG_H2D) / sizeof(uint32_t);  // command FIS size;
        cmdHeader->write = 0;                                                  // this is a read
        cmdHeader->prdtLength = 1;

        uint64_t cmdTablePhys = static_cast<uint64_t>(cmdHeader->commandTableBaseAddressUpper) << 32 |
                                static_cast<uint64_t>(cmdHeader->commandTableBaseAddress);

        auto* commandTable = static_cast<HBACommandTable*>(phys_to_virt(cmdTablePhys));
        memset(commandTable, 0, sizeof(HBACommandTable) + (cmdHeader->prdtLength - 1) * sizeof(HBAPRDTEntry));

        commandTable->prdtEntry[0].dataBaseAddress = static_cast<uint32_t>(dma_phys);
        commandTable->prdtEntry[0].dataBaseAddressUpper = static_cast<uint32_t>(dma_phys >> 32);
        commandTable->prdtEntry[0].byteCount = sectorCount * sector_size - 1;  // (sectorCount<<9)-1;
        commandTable->prdtEntry[0].interruptOnCompletion = 1;

        auto* cmdFIS = reinterpret_cast<FIS_REG_H2D*>(&commandTable->commandFIS);

        cmdFIS->fisType = FIS_TYPE_REG_H2D;
        cmdFIS->commandControl = 1;  // command
        cmdFIS->command = ATA_CMD_READ_DMA_EX;

        cmdFIS->lba0 = static_cast<uint8_t>(sectorL);
        cmdFIS->lba1 = static_cast<uint8_t>(sectorL >> 8);
        cmdFIS->lba2 = static_cast<uint8_t>(sectorL >> 16);
        cmdFIS->lba3 = static_cast<uint8_t>(sectorL >> 24);
        cmdFIS->lba4 = static_cast<uint8_t>(sectorH & 0xFF);
        cmdFIS->lba5 = static_cast<uint8_t>(sectorH >> 8 & 0xFF);

        cmdFIS->deviceRegister = 1 << 6;  // LBA mode

        cmdFIS->countLow = sectorCount & 0xFF;
        cmdFIS->countHigh = sectorCount >> 8 & 0xFF;

        commandCompleted = false;
        lastError = false;
        hbaPort->commandIssue = 1 << 0;

        while (!commandCompleted) {
            asm volatile("pause");  // TODO change this to yield
        }

        if (lastError) {
            kernel::memory::free_pages_phys(dma_phys, pages);
            Log::Error("[ AHCI ] Read disk error");
            return -EIO;
        }

        memcpy(buffer, dma, bytes);

        kernel::memory::free_pages_phys(dma_phys, pages);
        return static_cast<ssize_t>(bytes);
    }

    ssize_t Port::write(const uint64_t sector, const uint32_t sectorCount, void* buffer, size_t bufferSize) {
        size_t bytes = static_cast<size_t>(sectorCount) * sector_size;
        if (!buffer || sectorCount == 0 || bufferSize < bytes) return -EINVAL;
        kernel::mutex_guard guard(portMutex);

        size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        uint64_t dma_phys = kernel::memory::request_pages_phys(pages);
        void* dma = phys_to_virt(dma_phys);
        if (!dma_phys) return -ENOMEM;

        memcpy(dma, buffer, bytes);

        const auto sectorL = static_cast<uint32_t>(sector);
        const auto sectorH = static_cast<uint32_t>(sector >> 32);

        hbaPort->interruptStatus = static_cast<uint32_t>(-1);  // clear interrupts

        const uint64_t cmdListPhys = static_cast<uint64_t>(hbaPort->commandListBaseUpper) << 32 |
                                     static_cast<uint64_t>(hbaPort->commandListBase);
        auto* cmdHeader = static_cast<HBACommandHeader*>(phys_to_virt(cmdListPhys));
        cmdHeader->commandFISLength = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
        cmdHeader->write = 1;
        cmdHeader->prdtLength = 1;

        const uint64_t cmdTablePhys = static_cast<uint64_t>(cmdHeader->commandTableBaseAddressUpper) << 32 |
                                      static_cast<uint64_t>(cmdHeader->commandTableBaseAddress);

        auto* commandTable = static_cast<HBACommandTable*>(phys_to_virt(cmdTablePhys));
        memset(commandTable, 0, sizeof(HBACommandTable) + (cmdHeader->prdtLength - 1) * sizeof(HBAPRDTEntry));

        commandTable->prdtEntry[0].dataBaseAddress = static_cast<uint32_t>(dma_phys);
        commandTable->prdtEntry[0].dataBaseAddressUpper = static_cast<uint32_t>(dma_phys >> 32);
        commandTable->prdtEntry[0].byteCount = sectorCount * 512 - 1;
        commandTable->prdtEntry[0].interruptOnCompletion = 1;

        auto* cmdFIS = reinterpret_cast<FIS_REG_H2D*>(&commandTable->commandFIS);
        cmdFIS->fisType = FIS_TYPE_REG_H2D;
        cmdFIS->commandControl = 1;
        cmdFIS->command = ATA_CMD_WRITE_DMA_EX;

        cmdFIS->lba0 = static_cast<uint8_t>(sectorL);
        cmdFIS->lba1 = static_cast<uint8_t>(sectorL >> 8);
        cmdFIS->lba2 = static_cast<uint8_t>(sectorL >> 16);
        cmdFIS->lba3 = static_cast<uint8_t>(sectorL >> 24);
        cmdFIS->lba4 = static_cast<uint8_t>(sectorH & 0xFF);
        cmdFIS->lba5 = static_cast<uint8_t>(sectorH >> 8 & 0xFF);

        cmdFIS->deviceRegister = 1 << 6;  // LBA mode

        cmdFIS->countLow = sectorCount & 0xFF;
        cmdFIS->countHigh = sectorCount >> 8 & 0xFF;

        commandCompleted = false;
        lastError = false;
        hbaPort->commandIssue = 1 << 0;

        while (!commandCompleted) {
            asm volatile("pause");  // TODO change this to yield
        }

        if (lastError) {
            kernel::memory::free_pages_phys(dma_phys, pages);
            Log::Error("[ AHCI ] Read disk error");
            return -EIO;
        }

        kernel::memory::free_pages_phys(dma_phys, pages);
        return static_cast<ssize_t>(bytes);
    }

    AHCIDriver::AHCIDriver(PCI::PCIDeviceHeader* pciBaseAddress)
        : PCIBaseAddress(pciBaseAddress), portCount(0) {

        Log::Ok("[ AHCI ] AHCI Driver instance initialized");

        char name[16];
        DeviceManager::AllocUniqueDeviceName("ahci", name, sizeof(name));
        kd = DeviceManager::RegisterController(name, DeviceClass::Storage, BusType::BUS_PCI, ControllerType::AHCI);

        const uint64_t abar_phys = reinterpret_cast<PCI::PCIHeader0*>(pciBaseAddress)->BAR5;
        ABAR = static_cast<HBAMemory*>(phys_to_virt(abar_phys));
        kernel::memory::map_memory(
            ABAR, reinterpret_cast<void*>(abar_phys), (1ULL << PT_Flag::CacheDisabled) | (1ULL << PT_Flag::WriteThrough)
        );

        ProbePorts();

        const uint8_t vector = kernel::interrupts::get_free_vector();
        kernel::interrupts::allocate_vector(vector, reinterpret_cast<irq_handler_t>(GlobalInterruptHandler), this);
        if (!PCI::try_enable_msi_or_msix(reinterpret_cast<PCI::PCIHeader0*>(pciBaseAddress), vector)) {
            Log::debug("[ AHCI ] AHCI Driver instance failed to enable MSI");
            this->~AHCIDriver();
        };  // switch to polling later maybe

        ABAR->globalHostControl |= AHCI_GHC_AE;
        ABAR->globalHostControl |= AHCI_GHC_IE;

        for (int i = 0; i < portCount; i++) {
            Port* port = ports[i];

            port->vector = vector;

            port->Configure();

            port->EnableInterrupts();

            port->Identify();

            char name_buf[16] = {};
            DeviceManager::GenerateSDDeviceName(name_buf, sizeof(name_buf));
            port->kd = DeviceManager::RegisterBlockDevice(
                port, name_buf, DeviceClass::Storage, BusType::BUS_PCI, ControllerType::AHCI, kd
            );
            DevFS::register_device(port->kd);
            DeviceManager::FindAndRegisterPartitions(port->kd);
        }
    }

    bool AHCIDriver::HasActivePorts() const {
        return portCount > 0;
    }

    AHCIDriver::~AHCIDriver() {
        kernel::memory::unmap_memory(ABAR);

        for (int i = 0; i < portCount; i++) {
            Port* port = ports[i];
            delete port;
        }
    }
}  // namespace AHCI
