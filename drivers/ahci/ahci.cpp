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

namespace ahci {
#define HBA_PORT_DEV_PRESENT 0x3
#define HBA_PORT_IPM_ACTIVE 0x1
#define SATA_SIG_ATAPI 0xEB140101
#define SATA_SIG_ATA 0x00000101
#define SATA_SIG_SEMB 0xC33C0101
#define SATA_SIG_PM 0x96690101

#define HBA_PX_CMD_CR 0x8000
#define HBA_PX_CMD_FRE 0x0010
#define HBA_PX_CMD_ST 0x0001
#define HBA_PX_CMD_FR 0x4000

    PortType check_port_type(const HBA_PORT* port) {
        uint32_t sata_status = port->sata_status;

        const uint8_t interface_power_management = sata_status >> 8 & 0b111;

        if (const uint8_t device_detection = sata_status & 0b111; device_detection != HBA_PORT_DEV_PRESENT) return None;
        if (interface_power_management != HBA_PORT_IPM_ACTIVE) return None;

        switch (port->signature) {
            case SATA_SIG_ATAPI:
                return Satapi;
            case SATA_SIG_ATA:
                return Sata;
            case SATA_SIG_PM:
                return Pm;
            case SATA_SIG_SEMB:
                return Semb;
            default:
                return None;
        }
    }

    void AhciDriver::probe_ports() {
        for (int i = 0; i < 32; i++) {
            if (!(abar->ports_implemented & (1 << i))) continue;

            const PortType port_type = check_port_type(&abar->ports[i]);
            if (port_type != Sata && port_type != Satapi) continue;

            // we only want ports which have a device present with Phy communication established
            if (const uint32_t ssts = abar->ports[i].sata_status; (ssts & 0xF) != 3) continue;

            ports[port_count] = new Port();
            ports[port_count]->port_type = port_type;
            ports[port_count]->hba_port = &abar->ports[i];
            ports[port_count]->port_number = port_count;
            port_count++;
        }
    }

    void Port::interrupt_handler() {
        uint32_t is = hba_port->interrupt_status;

        if (!is) return;  // no Interrupt

        if (is & HBA_PX_IS_TFES) {
            last_error = true;
            Log::error("[AHCI] Port %u transfer error", port_number);
        }

        command_completed = true;
        hba_port->interrupt_status = is;
    }

    Irqreturn AhciDriver::global_interrupt_handler(const AhciDriver* driver) {
        const uint32_t is = driver->abar->interrupt_status;

        if (!is) return IRQ_NONE;  // no Interrupt

        for (int i = 0; i < driver->port_count; i++) {
            if (is & (1 << driver->ports[i]->port_number)) driver->ports[i]->interrupt_handler();
        }

        driver->abar->interrupt_status = is;
        return IRQ_HANDLED;
    }

    void Port::enable_interrupts() const {
        hba_port->interrupt_status = 0xFFFFFFFF;
        hba_port->interrupt_enable = 0xFFFFFFFF;
    }

    void Port::configure() const {
        stop_cmd();

        // Command list
        const phys_addr_t cmd_list_phys = kernel::memory::request_page_phys();
        auto* cmd_list_virt = static_cast<HBA_COMMAND_HEADER*>(virt_ptr(phys_to_virt(cmd_list_phys)));
        memset(cmd_list_virt, 0, 1024);

        hba_port->command_list_base = static_cast<uint32_t>(phys_raw(cmd_list_phys));
        hba_port->command_list_base_upper = static_cast<uint32_t>(phys_raw(cmd_list_phys) >> 32);

        // FIS base
        const phys_addr_t fis_phys = kernel::memory::request_page_phys();
        const virt_addr_t fis_virt = phys_to_virt(fis_phys);
        memset(fis_virt, 0, 256);

        hba_port->fis_base_address = static_cast<uint32_t>(phys_raw(fis_phys));
        hba_port->fis_base_address_upper = static_cast<uint32_t>(phys_raw(fis_phys) >> 32);

        // Command tables
        for (int i = 0; i < 32; i++) {
            cmd_list_virt[i].prdt_length = 8;

            const phys_addr_t table_phys = kernel::memory::request_page_phys();
            const phys_addr_t table_phys_offset = phys_add(table_phys, i << 8);
            const virt_addr_t table_virt = phys_to_virt(table_phys);
            memset(table_virt, 0, 256);

            cmd_list_virt[i].command_table_base_address = static_cast<uint32_t>(phys_raw(table_phys_offset));
            cmd_list_virt[i].command_table_base_address_upper = static_cast<uint32_t>(phys_raw(table_phys_offset) >> 32);
        }

        start_cmd();
    }

    Port::~Port() {
        if (!hba_port) return;

        stop_cmd();

        // Free command list
        if (hba_port->command_list_base || hba_port->command_list_base_upper) {
            const phys_addr_t cmd_list_phys =
                make_phys(static_cast<uint64_t>(hba_port->command_list_base_upper) << 32 | hba_port->command_list_base);

            // Free command tables before freeing the list
            const auto* cmd_header = static_cast<HBA_COMMAND_HEADER*>(virt_ptr(phys_to_virt(cmd_list_phys)));
            for (int i = 0; i < 32; i++) {
                if (!cmd_header[i].command_table_base_address && !cmd_header[i].command_table_base_address_upper) continue;
                const phys_addr_t table_phys = make_phys(
                    static_cast<uint64_t>(cmd_header[i].command_table_base_address_upper) << 32 |
                    cmd_header[i].command_table_base_address
                );
                kernel::memory::free_page_phys(table_phys);
            }

            kernel::memory::free_page_phys(cmd_list_phys);
        }

        // Free FIS base
        if (hba_port->fis_base_address || hba_port->fis_base_address_upper) {
            const phys_addr_t fis_phys =
                make_phys(static_cast<uint64_t>(hba_port->fis_base_address_upper) << 32 | hba_port->fis_base_address);
            kernel::memory::free_page_phys(fis_phys);
        }

        // Free identify buffer
        if (identify_) {
            const phys_addr_t identify_phys = virt_to_phys(make_virt(identify_));
            kernel::memory::free_page_phys(identify_phys);
            identify_ = nullptr;
        }

        DevFs::unregister_device(kd);
        DeviceManager::unregister_device(kd);
    }

    void Port::stop_cmd() const {
        hba_port->cmd_sts &= ~HBA_PX_CMD_ST;
        hba_port->cmd_sts &= ~HBA_PX_CMD_FRE;

        while (hba_port->cmd_sts & HBA_PX_CMD_FR) {
        }
        while (hba_port->cmd_sts & HBA_PX_CMD_CR) {
        }
    }

    void Port::start_cmd() const {
        while (hba_port->cmd_sts & HBA_PX_CMD_CR) {
        }

        hba_port->cmd_sts |= HBA_PX_CMD_FRE;
        hba_port->cmd_sts |= HBA_PX_CMD_ST;
    }

    size_t Port::get_size() const {
        return total_sectors * sector_size;
    }
    size_t Port::get_sector_size() const {
        return sector_size;
    }

    bool Port::identify() {
        const phys_addr_t identify_phys = kernel::memory::request_page_phys();
        identify_ = static_cast<IDENTIFY_DEVICE_DATA*>(virt_ptr(phys_to_virt(identify_phys)));
        memset(identify_, 0, 0x1000);

        kernel::MutexGuard guard(port_mutex_);
        hba_port->interrupt_status = static_cast<uint32_t>(-1);

        const phys_addr_t cmd_list_phys =
            make_phys(static_cast<uint64_t>(hba_port->command_list_base_upper) << 32 | hba_port->command_list_base);
        auto* cmd_header = static_cast<HBA_COMMAND_HEADER*>(virt_ptr(phys_to_virt(cmd_list_phys)));

        cmd_header->command_fis_length = sizeof(FisRegH2D) / sizeof(uint32_t);
        cmd_header->write = 0;
        cmd_header->prdt_length = 1;

        const phys_addr_t cmd_table_phys = make_phys(
            static_cast<uint64_t>(cmd_header->command_table_base_address_upper) << 32 | cmd_header->command_table_base_address
        );
        auto* cmd_table = static_cast<HbaCommandTable*>(virt_ptr(phys_to_virt(cmd_table_phys)));
        memset(cmd_table, 0, sizeof(HbaCommandTable) + (cmd_header->prdt_length - 1) * sizeof(HBA_PRDT_ENTRY));

        cmd_table->prdt_entry[0].data_base_address = static_cast<uint32_t>(phys_raw(identify_phys));
        cmd_table->prdt_entry[0].data_base_address_upper = static_cast<uint32_t>(phys_raw(identify_phys) >> 32);
        cmd_table->prdt_entry[0].byte_count = 511;
        cmd_table->prdt_entry[0].interrupt_on_completion = 1;

        auto* cmd_fis = reinterpret_cast<FisRegH2D*>(&cmd_table->command_fis);
        cmd_fis->fis_type = FIS_TYPE_REG_H2D;
        cmd_fis->command_control = 1;
        cmd_fis->command = ATA_CMD_IDENTIFY;

        hba_port->command_issue = 1;

        while (true) {
            if ((hba_port->command_issue & 1) == 0) break;
            if (hba_port->interrupt_status & HBA_PX_IS_TFES) {
                Log::error("[ AHCI ] IDENTIFY error");
                return false;
            }
        }

        if (identify_->physical_logical_sector_size.logical_sector_longer_than256_words) {
            const uint32_t words =
                identify_->words_per_logical_sector[0] | static_cast<uint32_t>(identify_->words_per_logical_sector[1]) << 16;
            sector_size = words * 2;
        } else {
            sector_size = 512;
        }

        if (identify_->additional_supported.extended_user_addressable_sectors_supported) {
            total_sectors = static_cast<uint64_t>(identify_->extended_number_of_user_addressable_sectors[1]) << 32 |
                            identify_->extended_number_of_user_addressable_sectors[0];
        } else {
            total_sectors = identify_->user_addressable_sectors;
        }

        return true;
    }

    ssize_t Port::read(const uint64_t lba, const size_t sector_count, void* buffer, size_t buffer_size) {
        size_t bytes = static_cast<size_t>(sector_count) * sector_size;
        if (!buffer || sector_count == 0 || buffer_size < bytes) return -EINVAL;

        kernel::MutexGuard guard(port_mutex_);

        size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        phys_addr_t dma_phys = kernel::memory::request_pages_phys(pages);
        if (phys_null(dma_phys)) return -ENOMEM;
        void* dma = virt_ptr(phys_to_virt(dma_phys));

        const auto sector_l = static_cast<uint32_t>(lba);
        const auto sector_h = static_cast<uint32_t>(lba >> 32);

        hba_port->interrupt_status = 0xFFFFFFFF;

        phys_addr_t cmd_list_phys =
            make_phys(static_cast<uint64_t>(hba_port->command_list_base_upper) << 32 | hba_port->command_list_base);
        auto* cmd_header = static_cast<HBA_COMMAND_HEADER*>(virt_ptr(phys_to_virt(cmd_list_phys)));
        cmd_header->command_fis_length = sizeof(FisRegH2D) / sizeof(uint32_t);
        cmd_header->write = 0;
        cmd_header->prdt_length = 1;

        phys_addr_t cmd_table_phys = make_phys(
            static_cast<uint64_t>(cmd_header->command_table_base_address_upper) << 32 | cmd_header->command_table_base_address
        );
        auto* cmd_table = static_cast<HbaCommandTable*>(virt_ptr(phys_to_virt(cmd_table_phys)));
        memset(cmd_table, 0, sizeof(HbaCommandTable) + (cmd_header->prdt_length - 1) * sizeof(HBA_PRDT_ENTRY));

        cmd_table->prdt_entry[0].data_base_address = static_cast<uint32_t>(phys_raw(dma_phys));
        cmd_table->prdt_entry[0].data_base_address_upper = static_cast<uint32_t>(phys_raw(dma_phys) >> 32);
        cmd_table->prdt_entry[0].byte_count = sector_count * sector_size - 1;
        cmd_table->prdt_entry[0].interrupt_on_completion = 1;

        auto* cmd_fis = reinterpret_cast<FisRegH2D*>(&cmd_table->command_fis);
        cmd_fis->fis_type = FIS_TYPE_REG_H2D;
        cmd_fis->command_control = 1;
        cmd_fis->command = ATA_CMD_READ_DMA_EX;

        cmd_fis->lba0 = static_cast<uint8_t>(sector_l);
        cmd_fis->lba1 = static_cast<uint8_t>(sector_l >> 8);
        cmd_fis->lba2 = static_cast<uint8_t>(sector_l >> 16);
        cmd_fis->lba3 = static_cast<uint8_t>(sector_l >> 24);
        cmd_fis->lba4 = static_cast<uint8_t>(sector_h & 0xFF);
        cmd_fis->lba5 = static_cast<uint8_t>(sector_h >> 8 & 0xFF);

        cmd_fis->device_register = 1 << 6;
        cmd_fis->count_low = sector_count & 0xFF;
        cmd_fis->count_high = sector_count >> 8 & 0xFF;

        command_completed = false;
        last_error = false;
        hba_port->command_issue = 1 << 0;

        while (!command_completed) asm volatile("pause");

        if (last_error) {
            kernel::memory::free_pages_phys(dma_phys, pages);
            Log::error("[ AHCI ] Read disk error");
            return -EIO;
        }

        memcpy(buffer, dma, bytes);
        kernel::memory::free_pages_phys(dma_phys, pages);
        return static_cast<ssize_t>(bytes);
    }

    ssize_t Port::write(const uint64_t sector, const size_t sector_count, void* buffer, size_t buffer_size) {
        size_t bytes = static_cast<size_t>(sector_count) * sector_size;
        if (!buffer || sector_count == 0 || buffer_size < bytes) return -EINVAL;

        kernel::MutexGuard guard(port_mutex_);

        size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        phys_addr_t dma_phys = kernel::memory::request_pages_phys(pages);
        if (phys_null(dma_phys)) return -ENOMEM;
        void* dma = virt_ptr(phys_to_virt(dma_phys));

        memcpy(dma, buffer, bytes);

        const auto sector_l = static_cast<uint32_t>(sector);
        const auto sector_h = static_cast<uint32_t>(sector >> 32);

        hba_port->interrupt_status = static_cast<uint32_t>(-1);

        phys_addr_t cmd_list_phys =
            make_phys(static_cast<uint64_t>(hba_port->command_list_base_upper) << 32 | hba_port->command_list_base);
        auto* cmd_header = static_cast<HBA_COMMAND_HEADER*>(virt_ptr(phys_to_virt(cmd_list_phys)));
        cmd_header->command_fis_length = sizeof(FisRegH2D) / sizeof(uint32_t);
        cmd_header->write = 1;
        cmd_header->prdt_length = 1;

        phys_addr_t cmd_table_phys = make_phys(
            static_cast<uint64_t>(cmd_header->command_table_base_address_upper) << 32 | cmd_header->command_table_base_address
        );
        auto* cmd_table = static_cast<HbaCommandTable*>(virt_ptr(phys_to_virt(cmd_table_phys)));
        memset(cmd_table, 0, sizeof(HbaCommandTable) + (cmd_header->prdt_length - 1) * sizeof(HBA_PRDT_ENTRY));

        cmd_table->prdt_entry[0].data_base_address = static_cast<uint32_t>(phys_raw(dma_phys));
        cmd_table->prdt_entry[0].data_base_address_upper = static_cast<uint32_t>(phys_raw(dma_phys) >> 32);
        cmd_table->prdt_entry[0].byte_count = sector_count * 512 - 1;
        cmd_table->prdt_entry[0].interrupt_on_completion = 1;

        auto* cmd_fis = reinterpret_cast<FisRegH2D*>(&cmd_table->command_fis);
        cmd_fis->fis_type = FIS_TYPE_REG_H2D;
        cmd_fis->command_control = 1;
        cmd_fis->command = ATA_CMD_WRITE_DMA_EX;

        cmd_fis->lba0 = static_cast<uint8_t>(sector_l);
        cmd_fis->lba1 = static_cast<uint8_t>(sector_l >> 8);
        cmd_fis->lba2 = static_cast<uint8_t>(sector_l >> 16);
        cmd_fis->lba3 = static_cast<uint8_t>(sector_l >> 24);
        cmd_fis->lba4 = static_cast<uint8_t>(sector_h & 0xFF);
        cmd_fis->lba5 = static_cast<uint8_t>(sector_h >> 8 & 0xFF);

        cmd_fis->device_register = 1 << 6;
        cmd_fis->count_low = sector_count & 0xFF;
        cmd_fis->count_high = sector_count >> 8 & 0xFF;

        command_completed = false;
        last_error = false;
        hba_port->command_issue = 1 << 0;

        while (!command_completed) asm volatile("pause");

        if (last_error) {
            kernel::memory::free_pages_phys(dma_phys, pages);
            Log::error("[ AHCI ] Write disk error");
            return -EIO;
        }

        kernel::memory::free_pages_phys(dma_phys, pages);
        return static_cast<ssize_t>(bytes);
    }

    AhciDriver::AhciDriver(pci::PCI_DEVICE_HEADER* pci_base_address)
        : pci_base_address(pci_base_address)
        , port_count(0) {
        Log::ok("[ AHCI ] AHCI Driver instance initialized");

        char name[16];
        DeviceManager::alloc_unique_device_name("ahci", name, sizeof(name));
        kd_ = DeviceManager::register_controller(name, DeviceClass::Storage, BusType::Pci, ControllerType::Ahci);

        const phys_addr_t abar_phys = make_phys(reinterpret_cast<pci::PCI_HEADER0*>(pci_base_address)->bar5);
        abar = static_cast<HBA_MEMORY*>(virt_ptr(phys_to_virt(abar_phys)));
        kernel::memory::map_memory(
            make_virt(abar), abar_phys, (1ULL << PtFlag::CacheDisabled) | (1ULL << PtFlag::WriteThrough)
        );

        probe_ports();

        const uint8_t vector = kernel::interrupts::get_free_vector();
        kernel::interrupts::allocate_vector(vector, reinterpret_cast<irq_handler_t>(global_interrupt_handler), this);
        if (!pci::try_enable_msi_or_msix(reinterpret_cast<pci::PCI_HEADER0*>(pci_base_address), vector)) {
            Log::debug("[ AHCI ] AHCI Driver instance failed to enable MSI");
            this->~AhciDriver();
        }

        abar->global_host_control |= AHCI_GHC_AE;
        abar->global_host_control |= AHCI_GHC_IE;

        for (int i = 0; i < port_count; i++) {
            Port* port = ports[i];
            port->vector = vector;
            port->configure();
            port->enable_interrupts();
            port->identify();

            char name_buf[16] = {};
            DeviceManager::generate_sd_device_name(name_buf, sizeof(name_buf));
            port->kd = DeviceManager::register_block_device(
                port, name_buf, DeviceClass::Storage, BusType::Pci, ControllerType::Ahci, kd_
            );
            DevFs::register_device(port->kd);
            DeviceManager::find_and_register_partitions(port->kd);
        }
    }

    bool AhciDriver::has_active_ports() const {
        return port_count > 0;
    }

    AhciDriver::~AhciDriver() {
        kernel::memory::unmap_memory(make_virt(abar));
        for (int i = 0; i < port_count; i++) delete ports[i];
    }
}  // namespace AHCI