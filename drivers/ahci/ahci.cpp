#include "ahci.h"

#include <uapi/vespera/dev/ioctl_smart.h>
#include <vespera/devices/device_manager.h>
#include <filesystem/devfs.h>
#include <vespera/interrupts.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera_errno.h>

#include <kernel/units/unit_manager.h>
#include "../pci/msi.h"
#include "../pci/msix.h"
#include "ata.h"
#include "vespera/scheduling.h"
#include "vespera/unit_config.h"

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
        const u32 sata_status = port->sata_status;

        const u8 interface_power_management = sata_status >> 8 & 0b111;

        if (const u8 device_detection = sata_status & 0b111; device_detection != HBA_PORT_DEV_PRESENT) return None;
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
            if (!(abar->ports_implemented & 1 << i)) continue;

            const PortType port_type = check_port_type(&abar->ports[i]);
            if (port_type != Sata && port_type != Satapi) continue;

            // we only want ports which have a device present with Phy communication established
            if (const u32 ssts = abar->ports[i].sata_status; (ssts & 0xF) != 3) continue;

            ports[port_count] = new Port();
            ports[port_count]->port_type = port_type;
            ports[port_count]->hba_port = &abar->ports[i];
            ports[port_count]->port_number = i;
            port_count++;
        }
    }

    void Port::interrupt_handler() {
        const u32 is = hba_port->interrupt_status;

        if (!is) return;  // no Interrupt

        if (is & HBA_PX_IS_TFES) {
            last_error = true;
            Log::error("[AHCI] Port %u transfer error", port_number);
        }

        command_completed = true;
        hba_port->interrupt_status = is;
    }

    Irqreturn AhciDriver::global_interrupt_handler(const AhciDriver* driver) {
        const u32 is = driver->abar->interrupt_status;

        if (!is) return IRQ_NONE;  // no Interrupt

        for (int i = 0; i < driver->port_count; i++) {
            if (is & 1 << driver->ports[i]->port_number) driver->ports[i]->interrupt_handler();
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

        hba_port->command_list_base = static_cast<u32>(phys_raw(cmd_list_phys));
        hba_port->command_list_base_upper = static_cast<u32>(phys_raw(cmd_list_phys) >> 32);

        // FIS base
        const phys_addr_t fis_phys = kernel::memory::request_page_phys();
        const virt_addr_t fis_virt = phys_to_virt(fis_phys);
        memset(fis_virt, 0, 256);

        hba_port->fis_base_address = static_cast<u32>(phys_raw(fis_phys));
        hba_port->fis_base_address_upper = static_cast<u32>(phys_raw(fis_phys) >> 32);

        // Command tables
        for (int i = 0; i < 32; i++) {
            cmd_list_virt[i].prdt_length = 8;

            const phys_addr_t table_phys = kernel::memory::request_page_phys();
            const phys_addr_t table_phys_offset = phys_add(table_phys, i << 8);
            const virt_addr_t table_virt = phys_to_virt(table_phys);
            memset(table_virt, 0, 256);

            cmd_list_virt[i].command_table_base_address = static_cast<u32>(phys_raw(table_phys_offset));
            cmd_list_virt[i].command_table_base_address_upper = static_cast<u32>(phys_raw(table_phys_offset) >> 32);
        }

        start_cmd();
    }

    Port::~Port() {
        if (!hba_port) return;

        stop_cmd();

        // Free command list
        if (hba_port->command_list_base || hba_port->command_list_base_upper) {
            const phys_addr_t cmd_list_phys =
                make_phys(static_cast<u64>(hba_port->command_list_base_upper) << 32 | hba_port->command_list_base);

            // Free command tables before freeing the list
            const auto* cmd_header = static_cast<HBA_COMMAND_HEADER*>(virt_ptr(phys_to_virt(cmd_list_phys)));
            for (int i = 0; i < 32; i++) {
                if (!cmd_header[i].command_table_base_address && !cmd_header[i].command_table_base_address_upper)
                    continue;
                const phys_addr_t table_phys = make_phys(
                    static_cast<u64>(cmd_header[i].command_table_base_address_upper) << 32 |
                    cmd_header[i].command_table_base_address
                );
                kernel::memory::free_page_phys(table_phys);
            }

            kernel::memory::free_page_phys(cmd_list_phys);
        }

        // Free FIS base
        if (hba_port->fis_base_address || hba_port->fis_base_address_upper) {
            const phys_addr_t fis_phys =
                make_phys(static_cast<u64>(hba_port->fis_base_address_upper) << 32 | hba_port->fis_base_address);
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

        stop_io_worker();
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
            asm volatile("pause");
        }

        hba_port->cmd_sts |= HBA_PX_CMD_FRE;
        hba_port->cmd_sts |= HBA_PX_CMD_ST;
    }

    usize Port::get_size() const {
        return total_sectors * sector_size;
    }
    usize Port::get_sector_size() const {
        return sector_size;
    }

    bool Port::smart_read_data(u8* out_buf) {
        if (!has_smart_) return false;

        kernel::MutexGuard guard(port_mutex_);

        const phys_addr_t dma_phys = kernel::memory::request_page_phys();
        void* dma = virt_ptr(phys_to_virt(dma_phys));
        memset(dma, 0, 512);

        hba_port->interrupt_status = 0xFFFFFFFF;

        const phys_addr_t cmd_list_phys =
            make_phys(static_cast<u64>(hba_port->command_list_base_upper) << 32 | hba_port->command_list_base);
        auto* cmd_header = static_cast<HBA_COMMAND_HEADER*>(virt_ptr(phys_to_virt(cmd_list_phys)));
        cmd_header->command_fis_length = sizeof(FIS_REG_H2D) / sizeof(u32);
        cmd_header->write = 0;
        cmd_header->prdt_length = 1;

        const phys_addr_t cmd_table_phys = make_phys(
            static_cast<u64>(cmd_header->command_table_base_address_upper) << 32 |
            cmd_header->command_table_base_address
        );
        auto* cmd_table = static_cast<HbaCommandTable*>(virt_ptr(phys_to_virt(cmd_table_phys)));
        memset(cmd_table, 0, sizeof(HbaCommandTable));

        cmd_table->prdt_entry[0].data_base_address = static_cast<u32>(phys_raw(dma_phys));
        cmd_table->prdt_entry[0].data_base_address_upper = static_cast<u32>(phys_raw(dma_phys) >> 32);
        cmd_table->prdt_entry[0].byte_count = 511;  // 512 - 1
        cmd_table->prdt_entry[0].interrupt_on_completion = 1;

        auto* cmd_fis = reinterpret_cast<FIS_REG_H2D*>(&cmd_table->command_fis);
        cmd_fis->fis_type = FIS_TYPE_REG_H2D;
        cmd_fis->command_control = 1;
        cmd_fis->command = ATA_CMD_SMART;            // 0xB0
        cmd_fis->feature_low = ATA_SMART_READ_DATA;  // 0xD0
        cmd_fis->lba1 = 0x4F;                        // SMART signature
        cmd_fis->lba2 = 0xC2;                        // SMART signature

        command_completed = false;
        last_error = false;
        hba_port->command_issue = 1 << 0;

        while (!command_completed) {
            kernel::scheduling::yield();
        }

        if (last_error) {
            kernel::memory::free_page_phys(dma_phys);
            return false;
        }

        memcpy(out_buf, dma, 512);
        kernel::memory::free_page_phys(dma_phys);
        return true;
    }

    bool Port::smart_get_common(smart_common* out) {
        smart_ata ata{};
        if (!smart_get_ata(&ata)) return false;

        out->driver_type = SMART_DRIVER_ATA;
        out->temperature_celsius = ata.temperature_celsius;
        out->power_on_hours = ata.power_on_hours;
        out->health_ok = ata.health_ok;
        out->critical_warning_raw = ata.health_ok ? 0 : 0xFF;  // synthesized
        return true;
    }

    bool Port::smart_get_ata(smart_ata* out) {
        u8 raw[512];
        if (!smart_read_data(raw)) return false;

        const auto* s = reinterpret_cast<const ATA_SMART_DATA*>(raw);
        const auto* area = &s->vendor_specific_0;

        out->version = area->version;
        out->attr_count = 0;
        out->temperature_celsius = 0;
        out->power_on_hours = 0;
        out->power_cycles = 0;
        out->reallocated_sectors = 0;
        out->pending_sectors = 0;
        out->uncorrectable_sectors = 0;
        out->health_ok = smart_return_status();

        for (const auto src : area->attributes) {
            if (src.id == 0) continue;

            smart_attribute& dst = out->attrs[out->attr_count++];
            dst.id = src.id;
            dst.flags = src.flags;
            dst.current = src.current;
            dst.worst = src.worst;
            dst.threshold = 0;  // vendor specific typeshit, is marked as obsolete in ACS-4
            memcpy(dst.raw, src.raw, 6);

            switch (src.id) {
                case 0x05:
                    out->reallocated_sectors = src.raw[0] | static_cast<u32>(src.raw[1]) << 8;
                    if (out->reallocated_sectors > 0) out->health_ok = 0;
                    break;
                case 0x09:
                    out->power_on_hours = src.raw[0] | static_cast<u64>(src.raw[1]) << 8 |
                                          static_cast<u64>(src.raw[2]) << 16 | static_cast<u64>(src.raw[3]) << 24;
                    break;
                case 0x0C:
                    out->power_cycles = src.raw[0] | static_cast<u32>(src.raw[1]) << 8;
                    break;
                case 0xBE:
                    out->temperature_celsius = src.raw[0];
                    break;
                case 0xC5:
                    out->pending_sectors = src.raw[0] | static_cast<u32>(src.raw[1]) << 8;
                    if (out->pending_sectors > 0) out->health_ok = 0;
                    break;
                case 0xC6:
                    out->uncorrectable_sectors = src.raw[0] | static_cast<u32>(src.raw[1]) << 8;
                    break;
                default:
                    break;
            }
        }
        return true;
    }

    bool Port::smart_return_status() {
        if (!has_smart_) return false;

        kernel::MutexGuard guard(port_mutex_);

        hba_port->interrupt_status = 0xFFFFFFFF;

        const phys_addr_t cmd_list_phys =
            make_phys(static_cast<u64>(hba_port->command_list_base_upper) << 32 | hba_port->command_list_base);
        auto* cmd_header = static_cast<HBA_COMMAND_HEADER*>(virt_ptr(phys_to_virt(cmd_list_phys)));
        cmd_header->command_fis_length = sizeof(FIS_REG_H2D) / sizeof(u32);
        cmd_header->write = 0;
        cmd_header->prdt_length = 0;

        const phys_addr_t cmd_table_phys = make_phys(
            static_cast<u64>(cmd_header->command_table_base_address_upper) << 32 |
            cmd_header->command_table_base_address
        );
        auto* cmd_table = static_cast<HbaCommandTable*>(virt_ptr(phys_to_virt(cmd_table_phys)));
        memset(cmd_table, 0, sizeof(HbaCommandTable));

        auto* cmd_fis = reinterpret_cast<FIS_REG_H2D*>(&cmd_table->command_fis);
        cmd_fis->fis_type = FIS_TYPE_REG_H2D;
        cmd_fis->command_control = 1;
        cmd_fis->command = ATA_CMD_SMART;
        cmd_fis->feature_low = ATA_SMART_RETURN_STATUS;  // 0xDA
        cmd_fis->lba1 = 0x4F;                            // C24Fh signature: lba1=4F, lba2=C2
        cmd_fis->lba2 = 0xC2;

        command_completed = false;
        last_error = false;
        hba_port->command_issue = 1 << 0;

        while (!command_completed) {
            kernel::scheduling::yield();
        }

        if (last_error) return false;

        const phys_addr_t fis_phys =
            make_phys(static_cast<u64>(hba_port->fis_base_address_upper) << 32 | hba_port->fis_base_address);
        const auto* dev_to_host =
            reinterpret_cast<const FIS_REG_D2H*>(static_cast<u8*>(virt_ptr(phys_to_virt(fis_phys))) + 0x40);

        // 0x4F / 0xC2 = healthy
        // 0xF4 / 0x2C = threshold exceeded
        const u8 lba_mid = dev_to_host->lba1;
        const u8 lba_high = dev_to_host->lba2;

        if (lba_mid == 0xF4 && lba_high == 0x2C) return false;  // threshold exceeded
        return true;                                            // 0x4F / 0xC2 = healthy
    }

    bool Port::trim(const TrimRange* ranges, usize count) {
        if (!has_trim_) return false;
        if (!ranges || count == 0) return false;

        kernel::MutexGuard guard(port_mutex_);

        const usize blocks = (count + 63) / 64;
        const usize pages = (blocks * 512 + PAGE_SIZE - 1) / PAGE_SIZE;

        const phys_addr_t dma_phys = kernel::memory::request_pages_phys(pages);
        if (phys_null(dma_phys)) return false;

        auto* payload = static_cast<ATA_DSM_RANGE*>(virt_ptr(phys_to_virt(dma_phys)));
        memset(payload, 0, blocks * 512);

        for (usize i = 0; i < count; i++) {
            payload[i].lba = ranges[i].lba;
            payload[i].count = static_cast<u16>(ranges[i].sector_count);
        }

        hba_port->interrupt_status = 0xFFFFFFFF;

        const phys_addr_t cmd_list_phys =
            make_phys(static_cast<u64>(hba_port->command_list_base_upper) << 32 | hba_port->command_list_base);
        auto* cmd_header = static_cast<HBA_COMMAND_HEADER*>(virt_ptr(phys_to_virt(cmd_list_phys)));
        cmd_header->command_fis_length = sizeof(FIS_REG_H2D) / sizeof(u32);
        cmd_header->write = 1;  // Host -> Device
        cmd_header->prdt_length = 1;

        const phys_addr_t cmd_table_phys = make_phys(
            static_cast<u64>(cmd_header->command_table_base_address_upper) << 32 |
            cmd_header->command_table_base_address
        );
        auto* cmd_table = static_cast<HbaCommandTable*>(virt_ptr(phys_to_virt(cmd_table_phys)));
        memset(cmd_table, 0, sizeof(HbaCommandTable));

        cmd_table->prdt_entry[0].data_base_address = static_cast<u32>(phys_raw(dma_phys));
        cmd_table->prdt_entry[0].data_base_address_upper = static_cast<u32>(phys_raw(dma_phys) >> 32);
        cmd_table->prdt_entry[0].byte_count = blocks * 512 - 1;  // 0-based
        cmd_table->prdt_entry[0].interrupt_on_completion = 1;

        auto* cmd_fis = reinterpret_cast<FIS_REG_H2D*>(&cmd_table->command_fis);
        cmd_fis->fis_type = FIS_TYPE_REG_H2D;
        cmd_fis->command_control = 1;
        cmd_fis->command = ATA_CMD_DATA_SET_MANAGEMENT;
        cmd_fis->feature_low = ATA_DSM_TRIM;  // Bit 0 = TRIM bit
        cmd_fis->feature_high = 0;            // DSM FUNCTION reserved if TRIM=1
        cmd_fis->count_low = static_cast<u8>(blocks & 0xFF);
        cmd_fis->count_high = static_cast<u8>((blocks >> 8) & 0xFF);
        cmd_fis->device_register = 0;

        command_completed = false;
        last_error = false;
        hba_port->command_issue = 1 << 0;

        while (!command_completed) {
            kernel::scheduling::yield();
        }

        kernel::memory::free_pages_phys(dma_phys, pages);

        if (last_error) {
            Log::error("[ AHCI ] Port %u: TRIM failed", port_number);
            return false;
        }

        for (usize i = 0; i < count; i++) {
            Log::debug("[FAT32] TRIM range %u: lba=%llu sectors=%u", i, ranges[i].lba, ranges[i].sector_count);
        }
        return true;
    }

    bool Port::flush() {
        if (!has_write_cache_) return true;

        kernel::MutexGuard guard(port_mutex_);

        hba_port->interrupt_status = 0xFFFFFFFF;

        const phys_addr_t cmd_list_phys =
            make_phys(static_cast<u64>(hba_port->command_list_base_upper) << 32 | hba_port->command_list_base);
        auto* cmd_header = static_cast<HBA_COMMAND_HEADER*>(virt_ptr(phys_to_virt(cmd_list_phys)));

        cmd_header->command_fis_length = sizeof(FIS_REG_H2D) / sizeof(u32);
        cmd_header->write = 0;
        cmd_header->prdt_length = 0;  // no data transfer

        const phys_addr_t cmd_table_phys = make_phys(
            static_cast<u64>(cmd_header->command_table_base_address_upper) << 32 |
            cmd_header->command_table_base_address
        );
        auto* cmd_table = static_cast<HbaCommandTable*>(virt_ptr(phys_to_virt(cmd_table_phys)));
        memset(cmd_table, 0, sizeof(HbaCommandTable));

        auto* cmd_fis = reinterpret_cast<FIS_REG_H2D*>(&cmd_table->command_fis);
        cmd_fis->fis_type = FIS_TYPE_REG_H2D;
        cmd_fis->command_control = 1;
        cmd_fis->command = has_flush_cache_ext_ ? ATA_CMD_FLUSH_CACHE_EXT : ATA_CMD_FLUSH_CACHE;

        command_completed = false;
        last_error = false;
        hba_port->command_issue = 1 << 0;

        while (!command_completed) {
            kernel::scheduling::yield();
        }

        if (last_error) {
            Log::error("[ AHCI ] Port %u: FLUSH CACHE failed", port_number);
            return false;
        }

        Log::debug("[ AHCI ] Port %u: FLUSH CACHE ok", port_number);
        return true;
    }

    bool Port::write_cache_enabled() const {
        return has_write_cache_;
    }

    bool Port::supports_trim() const {
        return has_trim_;
    }

    bool Port::get_model(char* out, const usize len) {
        if (!identify_) return false;
        copy_ata_string(out, len, identify_->model_number, 40);
        return true;
    }

    bool Port::get_serial(char* out, const usize len) {
        if (!identify_) return false;
        copy_ata_string(out, len, identify_->serial_number, 20);
        return true;
    }

    bool Port::get_firmware(char* out, const usize len) {
        if (!identify_) return false;
        copy_ata_string(out, len, identify_->firmware_revision, 8);
        return true;
    }

    void Port::copy_ata_string(char* dst, const usize dst_len, const u8* src, const usize src_chars) {
        const usize n = src_chars < dst_len - 1 ? src_chars : dst_len - 1;

        for (usize i = 0; i + 1 < n; i += 2) {
            dst[i] = static_cast<char>(src[i + 1]);
            dst[i + 1] = static_cast<char>(src[i]);
        }
        if (n % 2 != 0) dst[n - 1] = static_cast<char>(src[n - 1]);

        dst[n] = '\0';
        for (isize i = static_cast<isize>(n) - 1; i >= 0 && dst[i] == ' '; i--) dst[i] = '\0';
    }

    bool Port::identify() {
        const phys_addr_t identify_phys = kernel::memory::request_page_phys();
        identify_ = static_cast<IDENTIFY_DEVICE_DATA*>(virt_ptr(phys_to_virt(identify_phys)));
        memset(identify_, 0, 0x1000);

        kernel::MutexGuard guard(port_mutex_);
        hba_port->interrupt_status = static_cast<u32>(-1);

        const phys_addr_t cmd_list_phys =
            make_phys(static_cast<u64>(hba_port->command_list_base_upper) << 32 | hba_port->command_list_base);
        auto* cmd_header = static_cast<HBA_COMMAND_HEADER*>(virt_ptr(phys_to_virt(cmd_list_phys)));

        cmd_header->command_fis_length = sizeof(FIS_REG_H2D) / sizeof(u32);
        cmd_header->write = 0;
        cmd_header->prdt_length = 1;

        const phys_addr_t cmd_table_phys = make_phys(
            static_cast<u64>(cmd_header->command_table_base_address_upper) << 32 |
            cmd_header->command_table_base_address
        );
        auto* cmd_table = static_cast<HbaCommandTable*>(virt_ptr(phys_to_virt(cmd_table_phys)));
        memset(cmd_table, 0, sizeof(HbaCommandTable) + (cmd_header->prdt_length - 1) * sizeof(HBA_PRDT_ENTRY));

        cmd_table->prdt_entry[0].data_base_address = static_cast<u32>(phys_raw(identify_phys));
        cmd_table->prdt_entry[0].data_base_address_upper = static_cast<u32>(phys_raw(identify_phys) >> 32);
        cmd_table->prdt_entry[0].byte_count = 511;
        cmd_table->prdt_entry[0].interrupt_on_completion = 1;

        auto* cmd_fis = reinterpret_cast<FIS_REG_H2D*>(&cmd_table->command_fis);
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

        has_write_cache_ = identify_->command_set_support.write_cache && identify_->command_set_active.write_cache;

        has_flush_cache_ext_ =
            identify_->command_set_support.flush_cache_ext && identify_->command_set_active.flush_cache_ext;

        if (has_write_cache_) {
            Log::info(
                "[ AHCI ] Port %u: write cache enabled, flush_cache_ext=%s",
                port_number,
                has_flush_cache_ext_ ? "yes" : "no"
            );
        }

        if (identify_->physical_logical_sector_size.logical_sector_longer_than256_words) {
            const u32 words =
                identify_->words_per_logical_sector[0] | static_cast<u32>(identify_->words_per_logical_sector[1]) << 16;
            sector_size = words * 2;
        } else {
            sector_size = 512;
        }

        if (identify_->additional_supported.extended_user_addressable_sectors_supported) {
            total_sectors = static_cast<u64>(identify_->extended_number_of_user_addressable_sectors[1]) << 32 |
                            identify_->extended_number_of_user_addressable_sectors[0];
        } else {
            total_sectors = identify_->user_addressable_sectors;
        }

        has_smart_ = identify_->command_set_support.smart_commands && identify_->command_set_active.smart_commands;
        if (has_smart_) {
            Log::info("[ AHCI ] Port %u: SMART supported", port_number);
        }

        has_trim_ = identify_->data_set_management_feature.supports_trim;
        if (has_trim_) {
            Log::info("[ AHCI ] Port %u: TRIM supported", port_number);
        }

        return true;
    }

    isize Port::read(const u64 lba, const usize sector_count, void* buffer, const usize buffer_size) {
        BlockIoRequest req;
        req.op = BlockIoOp::Read;
        req.lba = lba;
        req.sector_count = sector_count;
        req.buffer = buffer;
        req.buffer_size = buffer_size;
        req.done.init(1, 0);

        io_queue_.submit(&req);
        req.done.wait();
        return req.result;
    }

    isize Port::write(const u64 sector, const usize sector_count, const void* buffer, const usize buffer_size) {
        BlockIoRequest req;
        req.op = BlockIoOp::Write;
        req.lba = sector;
        req.sector_count = sector_count;
        req.buffer = const_cast<void*>(buffer);
        req.buffer_size = buffer_size;
        req.done.init(1, 0);

        io_queue_.submit(&req);
        req.done.wait();
        return req.result;
    }

    isize Port::do_read(const u64 lba, const usize sector_count, void* buffer, const usize buffer_size) {
        const usize bytes = sector_count * sector_size;
        if (!buffer || sector_count == 0 || buffer_size < bytes) return -EINVAL;

        usize pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        phys_addr_t dma_phys = kernel::memory::request_pages_phys(pages);
        if (phys_null(dma_phys)) return -ENOMEM;
        void* dma = virt_ptr(phys_to_virt(dma_phys));

        const auto sector_l = static_cast<u32>(lba);
        const auto sector_h = static_cast<u32>(lba >> 32);

        hba_port->interrupt_status = 0xFFFFFFFF;

        const phys_addr_t cmd_list_phys =
            make_phys(static_cast<u64>(hba_port->command_list_base_upper) << 32 | hba_port->command_list_base);
        auto* cmd_header = static_cast<HBA_COMMAND_HEADER*>(virt_ptr(phys_to_virt(cmd_list_phys)));
        cmd_header->command_fis_length = sizeof(FIS_REG_H2D) / sizeof(u32);
        cmd_header->write = 0;
        cmd_header->prdt_length = 1;

        const phys_addr_t cmd_table_phys = make_phys(
            static_cast<u64>(cmd_header->command_table_base_address_upper) << 32 |
            cmd_header->command_table_base_address
        );
        auto* cmd_table = static_cast<HbaCommandTable*>(virt_ptr(phys_to_virt(cmd_table_phys)));
        memset(cmd_table, 0, sizeof(HbaCommandTable) + (cmd_header->prdt_length - 1) * sizeof(HBA_PRDT_ENTRY));

        cmd_table->prdt_entry[0].data_base_address = static_cast<u32>(phys_raw(dma_phys));
        cmd_table->prdt_entry[0].data_base_address_upper = static_cast<u32>(phys_raw(dma_phys) >> 32);
        cmd_table->prdt_entry[0].byte_count = sector_count * sector_size - 1;
        cmd_table->prdt_entry[0].interrupt_on_completion = 1;

        auto* cmd_fis = reinterpret_cast<FIS_REG_H2D*>(&cmd_table->command_fis);
        cmd_fis->fis_type = FIS_TYPE_REG_H2D;
        cmd_fis->command_control = 1;
        cmd_fis->command = ATA_CMD_READ_DMA_EX;
        cmd_fis->lba0 = static_cast<u8>(sector_l);
        cmd_fis->lba1 = static_cast<u8>(sector_l >> 8);
        cmd_fis->lba2 = static_cast<u8>(sector_l >> 16);
        cmd_fis->lba3 = static_cast<u8>(sector_l >> 24);
        cmd_fis->lba4 = static_cast<u8>(sector_h & 0xFF);
        cmd_fis->lba5 = static_cast<u8>(sector_h >> 8 & 0xFF);
        cmd_fis->device_register = 1 << 6;
        cmd_fis->count_low = sector_count & 0xFF;
        cmd_fis->count_high = sector_count >> 8 & 0xFF;

        command_completed = false;
        last_error = false;
        hba_port->command_issue = 1 << 0;

        while (!command_completed) {
            kernel::scheduling::yield();
        };

        if (last_error) {
            kernel::memory::free_pages_phys(dma_phys, pages);
            Log::error("[ AHCI ] Read disk error");
            return -EIO;
        }

        memcpy(buffer, dma, bytes);
        kernel::memory::free_pages_phys(dma_phys, pages);
        return static_cast<isize>(bytes);
    }

    isize Port::do_write(const u64 sector, const usize sector_count, const void* buffer, const usize buffer_size) {
        const usize bytes = sector_count * sector_size;
        if (!buffer || sector_count == 0 || buffer_size < bytes) return -EINVAL;

        usize pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
        phys_addr_t dma_phys = kernel::memory::request_pages_phys(pages);
        if (phys_null(dma_phys)) return -ENOMEM;
        void* dma = virt_ptr(phys_to_virt(dma_phys));

        memcpy(dma, buffer, bytes);

        const auto sector_l = static_cast<u32>(sector);
        const auto sector_h = static_cast<u32>(sector >> 32);

        hba_port->interrupt_status = static_cast<u32>(-1);

        const phys_addr_t cmd_list_phys =
            make_phys(static_cast<u64>(hba_port->command_list_base_upper) << 32 | hba_port->command_list_base);
        auto* cmd_header = static_cast<HBA_COMMAND_HEADER*>(virt_ptr(phys_to_virt(cmd_list_phys)));
        cmd_header->command_fis_length = sizeof(FIS_REG_H2D) / sizeof(u32);
        cmd_header->write = 1;
        cmd_header->prdt_length = 1;

        const phys_addr_t cmd_table_phys = make_phys(
            static_cast<u64>(cmd_header->command_table_base_address_upper) << 32 |
            cmd_header->command_table_base_address
        );
        auto* cmd_table = static_cast<HbaCommandTable*>(virt_ptr(phys_to_virt(cmd_table_phys)));
        memset(cmd_table, 0, sizeof(HbaCommandTable) + (cmd_header->prdt_length - 1) * sizeof(HBA_PRDT_ENTRY));

        cmd_table->prdt_entry[0].data_base_address = static_cast<u32>(phys_raw(dma_phys));
        cmd_table->prdt_entry[0].data_base_address_upper = static_cast<u32>(phys_raw(dma_phys) >> 32);
        cmd_table->prdt_entry[0].byte_count = sector_count * sector_size - 1;
        cmd_table->prdt_entry[0].interrupt_on_completion = 1;

        auto* cmd_fis = reinterpret_cast<FIS_REG_H2D*>(&cmd_table->command_fis);
        cmd_fis->fis_type = FIS_TYPE_REG_H2D;
        cmd_fis->command_control = 1;
        cmd_fis->command = ATA_CMD_WRITE_DMA_EX;
        cmd_fis->lba0 = static_cast<u8>(sector_l);
        cmd_fis->lba1 = static_cast<u8>(sector_l >> 8);
        cmd_fis->lba2 = static_cast<u8>(sector_l >> 16);
        cmd_fis->lba3 = static_cast<u8>(sector_l >> 24);
        cmd_fis->lba4 = static_cast<u8>(sector_h & 0xFF);
        cmd_fis->lba5 = static_cast<u8>(sector_h >> 8 & 0xFF);
        cmd_fis->device_register = 1 << 6;
        cmd_fis->count_low = sector_count & 0xFF;
        cmd_fis->count_high = sector_count >> 8 & 0xFF;

        command_completed = false;
        last_error = false;
        hba_port->command_issue = 1 << 0;

        while (!command_completed) {
            kernel::scheduling::yield();
        };

        if (last_error) {
            kernel::memory::free_pages_phys(dma_phys, pages);
            Log::error("[ AHCI ] Write disk error");
            return -EIO;
        }

        kernel::memory::free_pages_phys(dma_phys, pages);
        return static_cast<isize>(bytes);
    }

    void Port::start_io_worker(const u8 cpu_id) {
        io_queue_.init();

        char unit_name[24];
        snprintf(unit_name, sizeof(unit_name), "ahci-io-%u", port_number);

        const UnitConfig cfg = {
            .name = unit_name,
            .cpu_id = cpu_id,
            .priority = 4,
            .stack_size = 0x4000,
            .initial_handles = nullptr,
            .initial_handle_count = 0,
            .is_idle = false,
            .is_user = false,
            .user_stack_size = 0,
        };

        const Unit* unit = UnitManager::create(KERNEL_REALM_DRIVER, io_worker_entry, this, &cfg);
        if (!unit) {
            Log::error("[ AHCI ] Port %u: failed to spawn I/O worker", port_number);
        }
    }

    void Port::stop_io_worker() {
        io_queue_.shutdown();
    }

    void Port::io_worker_entry(void* arg) {
        auto* port = static_cast<Port*>(arg);

        while (true) {
            BlockIoRequest* req = port->io_queue_.dequeue_blocking();
            if (!req) break;  // shutdown() was called

            switch (req->op) {
                case BlockIoOp::Read:
                    req->result = port->do_read(req->lba, req->sector_count, req->buffer, req->buffer_size);
                    break;
                case BlockIoOp::Write:
                    req->result = port->do_write(req->lba, req->sector_count, req->buffer, req->buffer_size);
                    break;
                case BlockIoOp::Flush:
                    req->result = port->flush() ? 0 : -EIO;
                    break;
            }

            req->done.signal();
        }
    }

    AhciDriver::AhciDriver(pci::PCI_DEVICE_HEADER* pci_base_address)
        : pci_base_address(pci_base_address)
        , port_count(0) {
        Log::ok("[ AHCI ] AHCI Driver instance initialized");

        char name[16];
        DeviceManager::alloc_unique_device_name("ahci", name, sizeof(name));
        kd_ = DeviceManager::register_device(
            DeviceDescriptor{}
                .set_name(name)
                .set_type(DeviceType::Controller)
                .set_class(DeviceClass::Storage)
                .set_bus(BusType::Pci)
                .set_controller(ControllerType::Ahci)
                .with_lifecycle(this)
                .with_info(this)
        );

        const phys_addr_t abar_phys = make_phys(reinterpret_cast<pci::PCI_HEADER0*>(pci_base_address)->bar5);
        abar = static_cast<HBA_MEMORY*>(virt_ptr(phys_to_virt(abar_phys)));
        kernel::memory::map_memory(make_virt(abar), abar_phys, 1ULL << CacheDisabled | 1ULL << PtFlag::WriteThrough);

        probe_ports();

        const u8 vector = kernel::interrupts::get_free_vector();
        kernel::interrupts::allocate_vector(vector, reinterpret_cast<irq_handler_t>(global_interrupt_handler), this);
        if (!pci::try_enable_msi_or_msix(reinterpret_cast<pci::PCI_HEADER0*>(pci_base_address), vector)) {
            Log::debug("[ AHCI ] AHCI Driver instance failed to enable MSI");
            delete this;
        }

        abar->global_host_control |= AHCI_GHC_AE;
        abar->global_host_control |= AHCI_GHC_IE;

        for (int i = 0; i < port_count; i++) {
            Port* port = ports[i];
            port->vector = vector;
            port->configure();
            port->enable_interrupts();
            port->identify();
            port->start_io_worker(4);

            char name_buf[16] = {};
            DeviceManager::generate_sd_device_name(name_buf, sizeof(name_buf));
            port->kd = DeviceManager::register_device(
                DeviceDescriptor{}
                    .set_name(name_buf)
                    .set_type(DeviceType::Block)
                    .set_class(DeviceClass::Storage)
                    .set_bus(BusType::Pci)
                    .set_controller(ControllerType::Ahci)
                    .with_block(port)
                    .with_smart(port)
                    .with_info(port)
                    .with_parent(kd_)
            );
            DevFs::register_device(port->kd);
        }
    }

    bool AhciDriver::get_vendor(char* out, const usize len) {
        strncpy(out, pci::get_vendor_name(pci_base_address->vendor_id), len);
        out[len - 1] = '\0';
        return true;
    }

    bool AhciDriver::get_model(char* out, const usize len) {
        strncpy(out, pci::get_device_name(pci_base_address->vendor_id, pci_base_address->device_id), len);
        out[len - 1] = '\0';
        return true;
    }

    bool AhciDriver::has_active_ports() const {
        return port_count > 0;
    }

    AhciDriver::~AhciDriver() {
        DevFs::unregister_device(kd_);
        DeviceManager::unregister_device(kd_);
        kernel::memory::unmap_memory(make_virt(abar));
        for (int i = 0; i < port_count; i++) delete ports[i];
    }

    void AhciDriver::on_shutdown() {
        Log::info("[ AHCI ] Shutdown: flushing and stopping all ports...");

        for (int i = 0; i < port_count; i++) {
            Port* port = ports[i];

            port->flush();

            u32 timeout = 500000;
            while (port->hba_port->command_issue != 0 && --timeout) {
                asm volatile("pause");
            }
            if (timeout == 0) {
                Log::warning("[ AHCI ] Port %u: commands did not drain on shutdown", port->port_number);
            }

            port->stop_cmd();

            port->hba_port->interrupt_enable = 0;
            port->hba_port->interrupt_status = 0xFFFFFFFF;  // clear pending Bits  (W1C)
        }

        abar->global_host_control &= ~AHCI_GHC_IE;
        abar->global_host_control &= ~AHCI_GHC_AE;
        abar->interrupt_status = 0xFFFFFFFF;

        Log::ok("[ AHCI ] Shutdown complete");
    }

    void AhciDriver::on_suspend() {
        Log::info("[ AHCI ] Flushing all ports before suspend...");
        for (int i = 0; i < port_count; i++) {
            ports[i]->flush();
        }
    }
}  // namespace ahci