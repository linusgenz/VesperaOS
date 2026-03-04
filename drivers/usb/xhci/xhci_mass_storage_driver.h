// xhci_mass_storage_driver.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 02.09.25.
//
// This file is part of VesperaOS.
// 
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#ifndef VESPERAOS_XHCI_MASS_STORAGE_DRIVER_H
#define VESPERAOS_XHCI_MASS_STORAGE_DRIVER_H
#include "../../../kernel/devices/blockdevice.h"


class xhciMassStorageDriver final : public xhciUsbDeviceDriver, public BlockDevice
{
public:
    xhciMassStorageDriver() :
        sector_size(512), total_sectors(0), max_lun(0), current_tag(1)
    {
    }

    ~xhciMassStorageDriver() override = default;

    void detach() override;

    void on_startup(USB::xhciDriver* hcd, xhciDevice* dev) override;

    void on_event(USB::xhciDriver* hcd, xhciDevice* dev) override;

    // BlockDevice interface
    ssize_t read(uint64_t lba, size_t sectorCount, void* buffer, size_t bufferSize) override;

    ssize_t write(uint64_t lba, size_t sectorCount, void* buffer, size_t bufferSize) override;

    [[nodiscard]] size_t get_sector_size() const override { return sector_size; }

    [[nodiscard]] size_t get_size() const override
    {
        return total_sectors * sector_size;
    }

private:
    KernelDevice* kd = nullptr;

    USB::xhciDriver* hcd{};
    xhciDevice* device{};
    xhciEndpoint* bulk_in_endpoint{};
    xhciEndpoint* bulk_out_endpoint{};

    kernel::mutex_t io_mutex{};

    uint32_t sector_size;
    uint64_t total_sectors;
    uint32_t max_lun; // Logical Unit Number

    // SCSI Command structures
    struct CBW
    {
        // Command Block Wrapper
        uint32_t signature; // 0x43425355 ("USBC")
        uint32_t tag;
        uint32_t data_length;
        uint8_t flags; // Bit 7: Direction (0=Out, 1=In)
        uint8_t lun; // Logical Unit Number
        uint8_t cb_length; // Command Block Length
        uint8_t cb[16]; // Command Block
    } __attribute__((packed));

    struct CSW
    {
        // Command Status Wrapper
        uint32_t signature; // 0x53425355 ("USBS")
        uint32_t tag;
        uint32_t data_residue;
        uint8_t status; // 0=Success, 1=Failed, 2=Phase Error
    } __attribute__((packed));

    struct MassStorageTransfer
    {
        enum class Phase { Idle, SentCBW, DataPhase, ReceivedCSW, Completed, Error } phase = Phase::Idle;

        CBW cbw{};
        CSW csw{};

        void* data_buffer{};
        uint32_t data_length{};
        uint32_t actual_length{};
        bool is_input{};

        xhciEndpoint* endpoint{};

        bool done{};
        int status{};
    };

    MassStorageTransfer* current_transfer{};
    uint8_t inquiry_buffer[36]{};
    uint8_t capacity_buffer[8]{};

    MassStorageTransfer transfer_test_unit_ready;
    MassStorageTransfer transfer_inquiry;
    MassStorageTransfer transfer_capacity;
    MassStorageTransfer transfer_rw;

    enum class InitPhase
    {
        TestUnitReady,
        Inquiry,
        ReadCapacity,
        Completed
    };

    bool init_done = false;
    int init_status = -1;
    InitPhase init_phase = InitPhase::TestUnitReady;

    uint32_t current_tag;

    void scsi_inquiry();

    void scsi_read_capacity();

    void start_bulk_transfer(MassStorageTransfer* transfer);

    void scsi_test_unit_ready();

    void initialize_device();

    void handle_completed_transfer();
};

#endif //VESPERAOS_XHCI_MASS_STORAGE_DRIVER_H
