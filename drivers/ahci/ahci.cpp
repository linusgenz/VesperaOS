#include "ahci.h"
#include "../../include/log.h"
#include <kernel/memory.h>

namespace AHCI
{
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

    PortType CheckPortType(const HBAPort* port)
    {
        uint32_t sataStatus = port->sataStatus;

        const uint8_t interfacePowerManagement = sataStatus >> 8 & 0b111;

        if (const uint8_t deviceDetection = sataStatus & 0b111; deviceDetection != HBA_PORT_DEV_PRESENT)
            return
                None;
        if (interfacePowerManagement != HBA_PORT_IPM_ACTIVE) return None;

        switch (port->signature)
        {
        case SATA_SIG_ATAPI:
            Log::LogMsg("[ AHCI ] SATAPI");
            return SATAPI;
        case SATA_SIG_ATA:
            Log::LogMsg("[ AHCI ] SATA");
            return SATA;
        case SATA_SIG_PM:
            Log::LogMsg("[ AHCI ] PM");
            return PM;
        case SATA_SIG_SEMB:
            Log::LogMsg("[ AHCI ] SEMB");
            return SEMB;
        default:
            return None;
        }
    }

    void AHCIDriver::ProbePorts()
    {
        uint32_t portsImplemented = ABAR->portsImplemented;
        for (int i = 0; i < 32; i++)
        {
            if (portsImplemented & 1 << i)
            {
                PortType portType = CheckPortType(&ABAR->ports[i]);

                if (portType == SATA || portType == SATAPI)
                {
                    ports[portCount] = new Port();
                    ports[portCount]->portType = portType;
                    ports[portCount]->hbaPort = &ABAR->ports[i];
                    ports[portCount]->portNumber = portCount;
                    portCount++;
                }
            }
        }
    }

    void Port::Configure() const
    {
        StopCMD();

        void* newBase = kernel::memory::request_page();

        hbaPort->commandListBase = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(newBase));
        hbaPort->commandListBaseUpper = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(newBase) >> 32);

        auto* cmdHeader = reinterpret_cast<HBACommandHeader*>(
            reinterpret_cast<uintptr_t>(newBase)
        );

        memset(newBase, 0, 1024);

        void* fisBase = kernel::memory::request_page();
        hbaPort->fisBaseAddress = static_cast<uint32_t>(reinterpret_cast<uint64_t>(fisBase));
        hbaPort->fisBaseAddressUpper = static_cast<uint32_t>(reinterpret_cast<uint64_t>(fisBase) >> 32);
        memset(fisBase, 0, 256);


        for (int i = 0; i < 32; i++)
        {
            cmdHeader[i].prdtLength = 8;

            void* cmdTableAddress = kernel::memory::request_page();
            const uint64_t address = reinterpret_cast<uint64_t>(cmdTableAddress) + (i << 8);
            cmdHeader[i].commandTableBaseAddress = static_cast<uint32_t>(address);
            cmdHeader[i].commandTableBaseAddressUpper = static_cast<uint32_t>(address >> 32);
            memset(cmdTableAddress, 0, 256);
        }

        StartCMD();
    }

    void Port::StopCMD() const
    {
        hbaPort->cmdSts &= ~HBA_PxCMD_ST;
        hbaPort->cmdSts &= ~HBA_PxCMD_FRE;

        while (true)
        {
            if (hbaPort->cmdSts & HBA_PxCMD_FR) continue;
            if (hbaPort->cmdSts & HBA_PxCMD_CR) continue;

            break;
        }
    }

    void Port::StartCMD() const
    {
        while (hbaPort->cmdSts & HBA_PxCMD_CR)
        {
        }

        hbaPort->cmdSts |= HBA_PxCMD_FRE;
        hbaPort->cmdSts |= HBA_PxCMD_ST;
    }

    bool Port::Read(const uint64_t sector, const uint32_t sectorCount, void* buffer)
    {
        kernel::mutex_guard guard(portMutex);

        const auto sectorL = static_cast<uint32_t>(sector);
        const auto sectorH = static_cast<uint32_t>(sector >> 32);

        hbaPort->interruptStatus = static_cast<uint32_t>(-1); // Clear pending interrupt bits

        const uint64_t cmdListPhys =
            static_cast<uint64_t>(hbaPort->commandListBaseUpper) << 32 |
            static_cast<uint64_t>(hbaPort->commandListBase);
        auto* cmdHeader = reinterpret_cast<HBACommandHeader*>(cmdListPhys);
        cmdHeader->commandFISLength = sizeof(FIS_REG_H2D) / sizeof(uint32_t); //command FIS size;
        cmdHeader->write = 0; //this is a read
        cmdHeader->prdtLength = 1;

        uint64_t cmdTablePhys =
            static_cast<uint64_t>(cmdHeader->commandTableBaseAddressUpper) << 32 |
            static_cast<uint64_t>(cmdHeader->commandTableBaseAddress);

        auto* commandTable = reinterpret_cast<HBACommandTable*>(cmdTablePhys);
        memset(commandTable, 0, sizeof(HBACommandTable) + (cmdHeader->prdtLength - 1) * sizeof(HBAPRDTEntry));

        commandTable->prdtEntry[0].dataBaseAddress = static_cast<uint32_t>(reinterpret_cast<uint64_t>(buffer));
        commandTable->prdtEntry[0].dataBaseAddressUpper = static_cast<uint32_t>(reinterpret_cast<uint64_t>(buffer) >>
            32);
        commandTable->prdtEntry[0].byteCount = sectorCount * 512 - 1; // (sectorCount<<9)-1; // 512 bytes per sector
        commandTable->prdtEntry[0].interruptOnCompletion = 1;

        auto* cmdFIS = reinterpret_cast<FIS_REG_H2D*>(&commandTable->commandFIS);

        cmdFIS->fisType = FIS_TYPE_REG_H2D;
        cmdFIS->commandControl = 1; // command
        cmdFIS->command = ATA_CMD_READ_DMA_EX;

        cmdFIS->lba0 = static_cast<uint8_t>(sectorL);
        cmdFIS->lba1 = static_cast<uint8_t>(sectorL >> 8);
        cmdFIS->lba2 = static_cast<uint8_t>(sectorL >> 16);
        cmdFIS->lba3 = static_cast<uint8_t>(sectorL >> 24);
        cmdFIS->lba4 = static_cast<uint8_t>(sectorH & 0xFF);
        cmdFIS->lba5 = static_cast<uint8_t>(sectorH >> 8 & 0xFF);

        cmdFIS->deviceRegister = 1 << 6; //LBA mode

        cmdFIS->countLow = sectorCount & 0xFF;
        cmdFIS->countHigh = sectorCount >> 8 & 0xFF;

        uint64_t spin = 0;

        while (hbaPort->taskFileData & (ATA_DEV_BUSY | ATA_DEV_DRQ) && spin < 1000000)
        {
            spin++;
        }
        if (spin == 1000000)
        {
            Log::Warning("[ AHCI ] Port is hung");
            return false;
        }

        hbaPort->commandIssue = 1 << 0;

        while (true)
        {
            if (hbaPort->commandIssue == 0) break;
            if (hbaPort->interruptStatus & HBA_PxIS_TFES)
            {
                Log::Error("[ AHCI ] Read disk error");
                return false;
            }
        }

        if (hbaPort->interruptStatus & HBA_PxIS_TFES)
        {
            Log::Error("[ AHCI ] Read disk error");
            return false;
        }

        return true;
    }

    bool Port::Write(const uint64_t sector, uint32_t sectorCount, void* buffer)
    {
        kernel::mutex_guard guard(portMutex);

        const auto sectorL = static_cast<uint32_t>(sector);
        const auto sectorH = static_cast<uint32_t>(sector >> 32);

        hbaPort->interruptStatus = static_cast<uint32_t>(-1); // clear interrupts

        const uint64_t cmdListPhys =
            static_cast<uint64_t>(hbaPort->commandListBaseUpper) << 32 |
            static_cast<uint64_t>(hbaPort->commandListBase);
        auto* cmdHeader = reinterpret_cast<HBACommandHeader*>(cmdListPhys);
        cmdHeader->commandFISLength = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
        cmdHeader->write = 1;
        cmdHeader->prdtLength = 1;

        const uint64_t cmdTablePhys =
            static_cast<uint64_t>(cmdHeader->commandTableBaseAddressUpper) << 32 |
            static_cast<uint64_t>(cmdHeader->commandTableBaseAddress);

        auto* commandTable = reinterpret_cast<HBACommandTable*>(cmdTablePhys);
        memset(commandTable, 0, sizeof(HBACommandTable) + (cmdHeader->prdtLength - 1) * sizeof(HBAPRDTEntry));

        commandTable->prdtEntry[0].dataBaseAddress = static_cast<uint32_t>(reinterpret_cast<uint64_t>(buffer));
        commandTable->prdtEntry[0].dataBaseAddressUpper = static_cast<uint32_t>(reinterpret_cast<uint64_t>(buffer) >>
            32);
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

        cmdFIS->deviceRegister = 1 << 6; // LBA mode

        cmdFIS->countLow = sectorCount & 0xFF;
        cmdFIS->countHigh = sectorCount >> 8 & 0xFF;

        // wait until not busy
        uint64_t spin = 0;
        while (hbaPort->taskFileData & (ATA_DEV_BUSY | ATA_DEV_DRQ) && spin < 1000000)
        {
            spin++;
        }
        if (spin == 1000000)
        {
            Log::Warning("write timeout");
            return false;
        }

        hbaPort->commandIssue = 1 << 0;

        while (true)
        {
            if ((hbaPort->commandIssue & 1 << 0) == 0) break;
            if (hbaPort->interruptStatus & HBA_PxIS_TFES)
            {
                Log::Error("[ AHCI ] Write disk error");
                return false;
            }
        }

        if (hbaPort->interruptStatus & HBA_PxIS_TFES)
        {
            Log::Error("[ AHCI ] Write disk error");
            return false;
        }

        return true;
    }

    AHCIDriver::AHCIDriver(PCI::PCIDeviceHeader* pciBaseAddress) : portCount(0)
    {
        this->PCIBaseAddress = pciBaseAddress;
        Log::Ok("[ AHCI ] AHCI Driver instance initialized");

        ABAR = (HBAMemory*)((PCI::PCIHeader0*)pciBaseAddress)->BAR5;

        kernel::memory::map_memory(ABAR, ABAR);
        ProbePorts();

        for (int i = 0; i < portCount; i++)
        {
            Port* port = ports[i];

            port->Configure();
        }
    }

    bool AHCIDriver::HasActivePorts() const
    {
        Log::debug("portCount: %d", portCount);
        return portCount > 0;
    }

    AHCIDriver::~AHCIDriver()
    {
        kernel::memory::unmap_memory(ABAR);
    }
}
