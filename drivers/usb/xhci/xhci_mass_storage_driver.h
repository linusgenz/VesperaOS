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
#include <vespera/devices/block.h>
#include <vespera/devices/device_manager.h>

#include "xhci_endpoint.h"
#include "xhci_usb_device_driver.h"

class XhciMassStorageDriver final : public XhciUsbDeviceDriver, public BlockDevice
{
public:

    ~XhciMassStorageDriver() override = default;

    void detach() override;

    void on_startup(usb::XhciDriver* hcd, XhciDevice* dev) override;

    void on_event(usb::XhciDriver* hcd, XhciDevice* dev) override;

    // BlockDevice interface
    ssize_t read(uint64_t lba, size_t sector_count, void* buffer, size_t buffer_size) override;

    ssize_t write(uint64_t lba, size_t sector_count, void* buffer, size_t buffer_size) override;

    [[nodiscard]] size_t get_sector_size() const override { return sector_size_; }

    [[nodiscard]] size_t get_size() const override
    {
        return total_sectors_ * sector_size_;
    }

private:
    KernelDevice* kd_ = nullptr;

    usb::XhciDriver* hcd_{};
    XhciDevice* device_{};
    XhciEndpoint* bulk_in_endpoint_{};
    XhciEndpoint* bulk_out_endpoint_{};

    kernel::Mutex io_mutex_{};

    uint32_t sector_size_{512};
    uint64_t total_sectors_{0};
    uint32_t max_lun_{0}; // Logical Unit Number

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
        enum class Phase { Idle, SentCbw, DataPhase, ReceivedCsw, Completed, Error } phase = Phase::Idle;

        CBW cbw{};
        CSW csw{};

        void* data_buffer{};
        uint32_t data_length{};
        uint32_t actual_length{};
        bool is_input{};

        XhciEndpoint* endpoint{};

        bool done{};
        int status{};
    };

    MassStorageTransfer* current_transfer_{};
    uint8_t inquiry_buffer_[36]{};
    uint8_t capacity_buffer_[8]{};

    MassStorageTransfer transfer_test_unit_ready_;
    MassStorageTransfer transfer_inquiry_;
    MassStorageTransfer transfer_capacity_;
    MassStorageTransfer transfer_rw_;

    enum class InitPhase
    {
        TestUnitReady,
        Inquiry,
        ReadCapacity,
        Completed
    };

    bool init_done_ = false;
    int init_status_ = -1;
    InitPhase init_phase_ = InitPhase::TestUnitReady;

    uint32_t current_tag_{1};

    void scsi_inquiry();

    void scsi_read_capacity();

    void start_bulk_transfer(MassStorageTransfer* transfer);

    void scsi_test_unit_ready();

    void initialize_device();

    void handle_completed_transfer();
};

#endif //VESPERAOS_XHCI_MASS_STORAGE_DRIVER_H
