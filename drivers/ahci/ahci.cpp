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
            if (is & (1 << driver->ports[i]->portNumber)) driver->ports[i]->InterruptHandler();
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

        // Command list
        const phys_addr_t cmd_list_phys = kernel::memory::request_page_phys();
        auto* cmd_list_virt = static_cast<HBACommandHeader*>(virt_ptr(phys_to_virt(cmd_list_phys)));
        memset(cmd_list_virt, 0, 1024);

        hbaPort->commandListBase = static_cast<uint32_t>(phys_raw(cmd_list_phys));
        hbaPort->commandListBaseUpper = static_cast<uint32_t>(phys_raw(cmd_list_phys) >> 32);

        // FIS base
        const phys_addr_t fis_phys = kernel::memory::request_page_phys();
        const virt_addr_t fis_virt = phys_to_virt(fis_phys);
        memset(fis_virt, 0, 256);

        hbaPort->fisBaseAddress = static_cast<uint32_t>(phys_raw(fis_phys));
        hbaPort->fisBaseAddressUpper = static_cast<uint32_t>(phys_raw(fis_phys) >> 32);

        // Command tables
        for (int i = 0; i < 32; i++) {
            cmd_list_virt[i].prdtLength = 8;

            const phys_addr_t table_phys = kernel::memory::request_page_phys();
            const phys_addr_t table_phys_offset = phys_add(table_phys, i << 8);
            const virt_addr_t table_virt = phys_to_virt(table_phys);
            memset(table_virt, 0, 256);

            cmd_list_virt[i].commandTableBaseAddress = static_cast<uint32_t>(phys_raw(table_phys_offset));
            cmd_list_virt[i].commandTableBaseAddressUpper = static_cast<uint32_t>(phys_raw(table_phys_offset) >> 32);
        }

        StartCMD();
    }

    Port::~Port() {
        if (!hbaPort) return;

        StopCMD();

        // Free command list
        if (hbaPort->commandListBase || hbaPort->commandListBaseUpper) {
            const phys_addr_t cmd_list_phys =
                make_phys(static_cast<uint64_t>(hbaPort->commandListBaseUpper) << 32 | hbaPort->commandListBase);

            // Free command tables before freeing the list
            const auto* cmd_header = static_cast<HBACommandHeader*>(virt_ptr(phys_to_virt(cmd_list_phys)));
            for (int i = 0; i < 32; i++) {
                if (!cmd_header[i].commandTableBaseAddress && !cmd_header[i].commandTableBaseAddressUpper) continue;
                const phys_addr_t table_phys = make_phys(
                    static_cast<uint64_t>(cmd_header[i].commandTableBaseAddressUpper) << 32 |
                    cmd_header[i].commandTableBaseAddress
                );
                kernel::memory::free_page_phys(table_phys);
            }

            kernel::memory::free_page_phys(cmd_list_phys);
        }

        // Free FIS base
        if (hbaPort->fisBaseAddress || hbaPort->fisBaseAddressUpper) {
            const phys_addr_t fis_phys =
                make_phys(static_cast<uint64_t>(hbaPort->fisBaseAddressUpper) << 32 | hbaPort->fisBaseAddress);
            kernel::memory::free_page_phys(fis_phys);
        }

        // Free identify buffer
        if (identify) {
            const phys_addr_t identify_phys = virt_to_phys(make_virt(identify));
            kernel::memory::free_page_phys(identify_phys);
            identify = nullptr;
        }

        DevFS::unregister_device(kd);
        DeviceManager::UnregisterDevice(kd);
    }

    void Port::StopCMD() const {
        hbaPort->cmdSts &= ~HBA_PxCMD_ST;
        hbaPort->cmdSts &= ~HBA_PxCMD_FRE;

        while (hbaPort->cmdSts & HBA_PxCMD_FR) {
        }
        while (hbaPort->cmdSts & HBA_PxCMD_CR) {
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
    size_t Port::get_sector_size() const {
        return sector_size;
    }

    bool Port::Identify() {
        const phys_addr_t identify_phys = kernel::memory::request_page_phys();
        identify = static_cast<IDENTIFY_DEVICE_DATA*>(virt_ptr(phys_to_virt(identify_phys)));
        memset(identify, 0, 0x1000);

        kernel::mutex_guard guard(portMutex);
        hbaPort->interruptStatus = static_cast<uint32_t>(-1);

        const phys_addr_t cmd_list_phys =
            make_phys(static_cast<uint64_t>(hbaPort->commandListBaseUpper) << 32 | hbaPort->commandListBase);
        auto* cmd_header = static_cast<HBACommandHeader*>(virt_ptr(phys_to_virt(cmd_list_phys)));

        cmd_header->commandFISLength = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
        cmd_header->write = 0;
        cmd_header->prdtLength = 1;

        const phys_addr_t cmd_table_phys = make_phys(
            static_cast<uint64_t>(cmd_header->commandTableBaseAddressUpper) << 32 | cmd_header->commandTableBaseAddress
        );
        auto* cmd_table = static_cast<HBACommandTable*>(virt_ptr(phys_to_virt(cmd_table_phys)));
        memset(cmd_table, 0, sizeof(HBACommandTable) + (cmd_header->prdtLength - 1) * sizeof(HBAPRDTEntry));

        cmd_table->prdtEntry[0].dataBaseAddress = static_cast<uint32_t>(phys_raw(identify_phys));
        cmd_table->prdtEntry[0].dataBaseAddressUpper = static_cast<uint32_t>(phys_raw(identify_phys) >> 32);
        cmd_table->prdtEntry[0].byteCount = 511;
        cmd_table->prdtEntry[0].interruptOnCompletion = 1;

        auto* cmd_fis = reinterpret_cast<FIS_REG_H2D*>(&cmd_table->commandFIS);
        cmd_fis->fisType = FIS_TYPE_REG_H2D;
        cmd_fis->commandControl = 1;
        cmd_fis->command = ATA_CMD_IDENTIFY;

        hbaPort->commandIssue = 1;

        while (true) {
            if ((hbaPort->commandIssue & 1) == 0) break;
            if (hbaPort->interruptStatus & HBA_PxIS_TFES) {
                Log::Error("[ AHCI ] IDENTIFY error");
                return false;
            }
        }

        if (identify->PhysicalLogicalSectorSize.LogicalSectorLongerThan256Words) {
            const uint32_t words =
                identify->WordsPerLogicalSector[0] | static_cast<uint32_t>(identify->WordsPerLogicalSector[1]) << 16;
            sector_size = words * 2;
        } else {
            sector_size = 512;
        }

        if (identify->AdditionalSupported.ExtendedUserAddressableSectorsSupported) {
            total_sectors = static_cast<uint64_t>(identify->ExtendedNumberOfUserAddressableSectors[1]) << 32 |
                            identify->ExtendedNumberOfUserAddressableSectors[0];
        } else {
            total_sectors = identify->UserAddressableSectors;
        }

        return true;
    }

    ssize_t Port::read(const uint64_t lba, const size_t sectorCount, void* buffer, size_t bufferSize) {
        size_t bytes = static_cast<size_t>(sectorCount) * sector_size;
        if (!buffer || sectorCount == 0 || bufferSize < bytes) return -EINVAL;

        kernel::mutex_guard guard(portMutex);

        size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        phys_addr_t dma_phys = kernel::memory::request_pages_phys(pages);
        if (phys_null(dma_phys)) return -ENOMEM;
        void* dma = virt_ptr(phys_to_virt(dma_phys));

        const auto sectorL = static_cast<uint32_t>(lba);
        const auto sectorH = static_cast<uint32_t>(lba >> 32);

        hbaPort->interruptStatus = 0xFFFFFFFF;

        phys_addr_t cmd_list_phys =
            make_phys(static_cast<uint64_t>(hbaPort->commandListBaseUpper) << 32 | hbaPort->commandListBase);
        auto* cmd_header = static_cast<HBACommandHeader*>(virt_ptr(phys_to_virt(cmd_list_phys)));
        cmd_header->commandFISLength = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
        cmd_header->write = 0;
        cmd_header->prdtLength = 1;

        phys_addr_t cmd_table_phys = make_phys(
            static_cast<uint64_t>(cmd_header->commandTableBaseAddressUpper) << 32 | cmd_header->commandTableBaseAddress
        );
        auto* cmd_table = static_cast<HBACommandTable*>(virt_ptr(phys_to_virt(cmd_table_phys)));
        memset(cmd_table, 0, sizeof(HBACommandTable) + (cmd_header->prdtLength - 1) * sizeof(HBAPRDTEntry));

        cmd_table->prdtEntry[0].dataBaseAddress = static_cast<uint32_t>(phys_raw(dma_phys));
        cmd_table->prdtEntry[0].dataBaseAddressUpper = static_cast<uint32_t>(phys_raw(dma_phys) >> 32);
        cmd_table->prdtEntry[0].byteCount = sectorCount * sector_size - 1;
        cmd_table->prdtEntry[0].interruptOnCompletion = 1;

        auto* cmd_fis = reinterpret_cast<FIS_REG_H2D*>(&cmd_table->commandFIS);
        cmd_fis->fisType = FIS_TYPE_REG_H2D;
        cmd_fis->commandControl = 1;
        cmd_fis->command = ATA_CMD_READ_DMA_EX;

        cmd_fis->lba0 = static_cast<uint8_t>(sectorL);
        cmd_fis->lba1 = static_cast<uint8_t>(sectorL >> 8);
        cmd_fis->lba2 = static_cast<uint8_t>(sectorL >> 16);
        cmd_fis->lba3 = static_cast<uint8_t>(sectorL >> 24);
        cmd_fis->lba4 = static_cast<uint8_t>(sectorH & 0xFF);
        cmd_fis->lba5 = static_cast<uint8_t>(sectorH >> 8 & 0xFF);

        cmd_fis->deviceRegister = 1 << 6;
        cmd_fis->countLow = sectorCount & 0xFF;
        cmd_fis->countHigh = sectorCount >> 8 & 0xFF;

        commandCompleted = false;
        lastError = false;
        hbaPort->commandIssue = 1 << 0;

        while (!commandCompleted) asm volatile("pause");

        if (lastError) {
            kernel::memory::free_pages_phys(dma_phys, pages);
            Log::Error("[ AHCI ] Read disk error");
            return -EIO;
        }

        memcpy(buffer, dma, bytes);
        kernel::memory::free_pages_phys(dma_phys, pages);
        return static_cast<ssize_t>(bytes);
    }

    ssize_t Port::write(const uint64_t sector, const size_t sectorCount, void* buffer, size_t bufferSize) {
        size_t bytes = static_cast<size_t>(sectorCount) * sector_size;
        if (!buffer || sectorCount == 0 || bufferSize < bytes) return -EINVAL;

        kernel::mutex_guard guard(portMutex);

        size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        phys_addr_t dma_phys = kernel::memory::request_pages_phys(pages);
        if (phys_null(dma_phys)) return -ENOMEM;
        void* dma = virt_ptr(phys_to_virt(dma_phys));

        memcpy(dma, buffer, bytes);

        const auto sectorL = static_cast<uint32_t>(sector);
        const auto sectorH = static_cast<uint32_t>(sector >> 32);

        hbaPort->interruptStatus = static_cast<uint32_t>(-1);

        phys_addr_t cmd_list_phys =
            make_phys(static_cast<uint64_t>(hbaPort->commandListBaseUpper) << 32 | hbaPort->commandListBase);
        auto* cmd_header = static_cast<HBACommandHeader*>(virt_ptr(phys_to_virt(cmd_list_phys)));
        cmd_header->commandFISLength = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
        cmd_header->write = 1;
        cmd_header->prdtLength = 1;

        phys_addr_t cmd_table_phys = make_phys(
            static_cast<uint64_t>(cmd_header->commandTableBaseAddressUpper) << 32 | cmd_header->commandTableBaseAddress
        );
        auto* cmd_table = static_cast<HBACommandTable*>(virt_ptr(phys_to_virt(cmd_table_phys)));
        memset(cmd_table, 0, sizeof(HBACommandTable) + (cmd_header->prdtLength - 1) * sizeof(HBAPRDTEntry));

        cmd_table->prdtEntry[0].dataBaseAddress = static_cast<uint32_t>(phys_raw(dma_phys));
        cmd_table->prdtEntry[0].dataBaseAddressUpper = static_cast<uint32_t>(phys_raw(dma_phys) >> 32);
        cmd_table->prdtEntry[0].byteCount = sectorCount * 512 - 1;
        cmd_table->prdtEntry[0].interruptOnCompletion = 1;

        auto* cmd_fis = reinterpret_cast<FIS_REG_H2D*>(&cmd_table->commandFIS);
        cmd_fis->fisType = FIS_TYPE_REG_H2D;
        cmd_fis->commandControl = 1;
        cmd_fis->command = ATA_CMD_WRITE_DMA_EX;

        cmd_fis->lba0 = static_cast<uint8_t>(sectorL);
        cmd_fis->lba1 = static_cast<uint8_t>(sectorL >> 8);
        cmd_fis->lba2 = static_cast<uint8_t>(sectorL >> 16);
        cmd_fis->lba3 = static_cast<uint8_t>(sectorL >> 24);
        cmd_fis->lba4 = static_cast<uint8_t>(sectorH & 0xFF);
        cmd_fis->lba5 = static_cast<uint8_t>(sectorH >> 8 & 0xFF);

        cmd_fis->deviceRegister = 1 << 6;
        cmd_fis->countLow = sectorCount & 0xFF;
        cmd_fis->countHigh = sectorCount >> 8 & 0xFF;

        commandCompleted = false;
        lastError = false;
        hbaPort->commandIssue = 1 << 0;

        while (!commandCompleted) asm volatile("pause");

        if (lastError) {
            kernel::memory::free_pages_phys(dma_phys, pages);
            Log::Error("[ AHCI ] Write disk error");
            return -EIO;
        }

        kernel::memory::free_pages_phys(dma_phys, pages);
        return static_cast<ssize_t>(bytes);
    }

    AHCIDriver::AHCIDriver(PCI::PCIDeviceHeader* pciBaseAddress)
        : PCIBaseAddress(pciBaseAddress)
        , portCount(0) {
        Log::Ok("[ AHCI ] AHCI Driver instance initialized");

        char name[16];
        DeviceManager::AllocUniqueDeviceName("ahci", name, sizeof(name));
        kd = DeviceManager::RegisterController(name, DeviceClass::Storage, BusType::BUS_PCI, ControllerType::AHCI);

        const phys_addr_t abar_phys = make_phys(reinterpret_cast<PCI::PCIHeader0*>(pciBaseAddress)->BAR5);
        ABAR = static_cast<HBAMemory*>(virt_ptr(phys_to_virt(abar_phys)));
        kernel::memory::map_memory(
            make_virt(ABAR), abar_phys, (1ULL << PT_Flag::CacheDisabled) | (1ULL << PT_Flag::WriteThrough)
        );

        ProbePorts();

        const uint8_t vector = kernel::interrupts::get_free_vector();
        kernel::interrupts::allocate_vector(vector, reinterpret_cast<irq_handler_t>(GlobalInterruptHandler), this);
        if (!PCI::try_enable_msi_or_msix(reinterpret_cast<PCI::PCIHeader0*>(pciBaseAddress), vector)) {
            Log::debug("[ AHCI ] AHCI Driver instance failed to enable MSI");
            this->~AHCIDriver();
        }

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
        kernel::memory::unmap_memory(make_virt(ABAR));
        for (int i = 0; i < portCount; i++) delete ports[i];
    }
}  // namespace AHCI