// xhci_mass_storage_driver.cpp
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

#include "xhci_mass_storage_driver.h"

#include <filesystem/devfs.h>
#include <vespera/log.h>

#include "vespera/devices/device_manager.h"
#include "vespera/scheduling.h"
#include "xhci.h"

void XhciMassStorageDriver::on_startup(usb::XhciDriver* hcd, XhciDevice* dev) {
    this->hcd_ = hcd;
    this->device_ = dev;

    init_status_ = -1;
    init_phase_ = InitPhase::TestUnitReady;
    init_semaphore_.init(1, 0);
    io_mutex_.init();

    initialize_device();

    if (!init_semaphore_.wait(5000)) {
        Log::error("Mass storage init timed out");
        return;
    }

    if (init_status_ == 0) {
        char name_buf[16] = {};
        DeviceManager::generate_sd_device_name(name_buf, sizeof(name_buf));
        kd_ = DeviceManager::register_device(
            DeviceDescriptor{}
                .set_name(name_buf)
                .set_type(DeviceType::Block)
                .set_class(DeviceClass::Storage)
                .set_bus(BusType::Usb)
                .set_controller(ControllerType::Xhci)
                .with_block(this)
                .with_parent(hcd->get_device())
                .with_info(device_info_)
                .with_usb_info(device_info_)
        );
        DevFs::register_device(kd_);
        Log::ok("USB Mass Storage initialized: %u sectors, %u bytes/sector", total_sectors_, sector_size_);
    } else {
        Log::error("Mass storage initialization failed");
    }
}

void XhciMassStorageDriver::on_event(usb::XhciDriver* hcd, XhciDevice* dev) {
    if (!current_transfer_) return;

    switch (current_transfer_->phase) {
        case MassStorageTransfer::Phase::SentCbw: {
            if (current_transfer_->data_length > 0) {
                // Data phase: select endpoint according to direction
                auto* data_ep = current_transfer_->is_input ? bulk_in_endpoint_ : bulk_out_endpoint_;

                xhci_trb_t data_trb{};
                data_trb.parameter = xhci_get_physical_addr(current_transfer_->data_buffer);
                data_trb.status = current_transfer_->data_length & 0x1FFFF;
                data_trb.control = (XHCI_TRB_TYPE_NORMAL << 10) | (1 << 0) | (1 << 5);
                if (current_transfer_->is_input) {
                    data_trb.control |= (1 << 16);  // IN for data phase if necessary
                }
                data_ep->get_transfer_ring()->enqueue(&data_trb);
                hcd->ring_doorbell(dev->get_slot_id(), data_ep->xhc_endpoint_num);

                current_transfer_->phase = MassStorageTransfer::Phase::DataPhase;
            } else {
                // No data phase → directly CSW (always IN)
                auto* ep_in = bulk_in_endpoint_;

                xhci_trb_t csw_trb{};
                csw_trb.parameter = xhci_get_physical_addr(&current_transfer_->csw);
                csw_trb.status = sizeof(CSW) & 0x1FFFF;
                csw_trb.control = (XHCI_TRB_TYPE_NORMAL << 10) | (1 << 0) | (1 << 5) | (1 << 16);  // IN
                ep_in->get_transfer_ring()->enqueue(&csw_trb);
                hcd->ring_doorbell(dev->get_slot_id(), ep_in->xhc_endpoint_num);

                current_transfer_->phase = MassStorageTransfer::Phase::ReceivedCsw;
            }
            break;
        }

        case MassStorageTransfer::Phase::DataPhase: {
            // Data phase complete → Request CSW (always IN)
            auto* ep_in = bulk_in_endpoint_;

            xhci_trb_t csw_trb{};
            csw_trb.parameter = xhci_get_physical_addr(&current_transfer_->csw);
            csw_trb.status = sizeof(CSW) & 0x1FFFF;
            csw_trb.control = (XHCI_TRB_TYPE_NORMAL << 10) | (1 << 0) | (1 << 5) | (1 << 16);  // IN
            ep_in->get_transfer_ring()->enqueue(&csw_trb);
            hcd->ring_doorbell(dev->get_slot_id(), ep_in->xhc_endpoint_num);

            current_transfer_->phase = MassStorageTransfer::Phase::ReceivedCsw;
            break;
        }

        case MassStorageTransfer::Phase::ReceivedCsw: {
            if (current_transfer_->csw.status == 0) {
                current_transfer_->phase = MassStorageTransfer::Phase::Completed;
            } else {
                current_transfer_->phase = MassStorageTransfer::Phase::Error;
            }
            handle_completed_transfer();
            break;
        }

        default:
            break;
    }
}

void XhciMassStorageDriver::initialize_device() {
    for (const auto& ep : interface_->endpoints) {
        if ((ep->usb_endpoint_attributes & 0x03) == 0x02) {
            if (ep->usb_endpoint_addr & 0x80)
                bulk_in_endpoint_ = ep;
            else
                bulk_out_endpoint_ = ep;
        }
    }

    if (!bulk_in_endpoint_ || !bulk_out_endpoint_) {
        Log::error("USB Mass Storage: Required bulk endpoints missing");
        return;
    }

    scsi_test_unit_ready();
}

void XhciMassStorageDriver::handle_completed_transfer() {
    if (!current_transfer_) return;

    if (init_phase_ == InitPhase::TestUnitReady) {
        current_transfer_ = nullptr;
        init_phase_ = InitPhase::Inquiry;
        scsi_inquiry();
    } else if (init_phase_ == InitPhase::Inquiry) {
        current_transfer_ = nullptr;
        init_phase_ = InitPhase::ReadCapacity;
        scsi_read_capacity();
    } else if (init_phase_ == InitPhase::ReadCapacity) {
        const auto* data = capacity_buffer_;
        const u32 last_lba = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
        const u32 block_size = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];

        total_sectors_ = last_lba + 1;
        sector_size_ = block_size;

        current_transfer_ = nullptr;
        init_phase_ = InitPhase::Completed;

        init_status_ = 0;
        init_semaphore_.signal();
    } else {
        current_transfer_->actual_length = current_transfer_->cbw.data_length - current_transfer_->csw.data_residue;

        if (current_transfer_->phase == MassStorageTransfer::Phase::Completed) {
            current_transfer_->status = 0;
        } else {
            current_transfer_->status = -1;
        }

        current_transfer_->done = true;
        current_transfer_ = nullptr;
    }
}

void XhciMassStorageDriver::scsi_test_unit_ready() {
    auto& transfer = transfer_test_unit_ready_;
    memset(&transfer, 0, sizeof(transfer));

    transfer.endpoint = bulk_out_endpoint_;  // OUT for CBW
    transfer.data_buffer = nullptr;
    transfer.data_length = 0;
    transfer.is_input = false;

    transfer.cbw.signature = 0x43425355;
    transfer.cbw.tag = current_tag_++;
    transfer.cbw.data_length = 0;
    transfer.cbw.flags = 0x00;
    transfer.cbw.lun = 0;
    transfer.cbw.cb_length = 6;
    transfer.cbw.cb[0] = 0x00;  // TEST UNIT READY

    start_bulk_transfer(&transfer);

    // The result is processed in on_event
}

void XhciMassStorageDriver::scsi_inquiry() {
    auto& transfer = transfer_inquiry_;
    memset(&transfer, 0, sizeof(transfer));

    transfer.endpoint = bulk_in_endpoint_;
    transfer.data_buffer = inquiry_buffer_;
    transfer.data_length = sizeof(inquiry_buffer_);
    transfer.is_input = true;

    transfer.cbw.signature = 0x43425355;
    transfer.cbw.tag = current_tag_++;
    transfer.cbw.data_length = sizeof(inquiry_buffer_);
    transfer.cbw.flags = 0x80;  // Data-In
    transfer.cbw.cb_length = 6;
    transfer.cbw.cb[0] = 0x12;  // INQUIRY
    transfer.cbw.cb[4] = sizeof(inquiry_buffer_);

    start_bulk_transfer(&transfer);

    // The result is processed in on_event
}

void XhciMassStorageDriver::scsi_read_capacity() {
    auto& transfer = transfer_capacity_;
    memset(&transfer, 0, sizeof(transfer));

    transfer.endpoint = bulk_in_endpoint_;
    transfer.data_buffer = capacity_buffer_;
    transfer.data_length = sizeof(capacity_buffer_);
    transfer.is_input = true;

    transfer.cbw.signature = 0x43425355;
    transfer.cbw.tag = current_tag_++;
    transfer.cbw.data_length = sizeof(capacity_buffer_);
    transfer.cbw.flags = 0x80;  // Data-In
    transfer.cbw.lun = 0;
    transfer.cbw.cb_length = 10;
    transfer.cbw.cb[0] = 0x25;  // READ CAPACITY(10)

    start_bulk_transfer(&transfer);

    // The result is processed in on_event
}

void XhciMassStorageDriver::start_bulk_transfer(MassStorageTransfer* transfer) {
    current_transfer_ = transfer;
    transfer->phase = MassStorageTransfer::Phase::SentCbw;

    const auto* ep_out = bulk_out_endpoint_;

    xhci_trb_t cbw_trb{};
    cbw_trb.parameter = xhci_get_physical_addr(&transfer->cbw);
    cbw_trb.status = sizeof(CBW) & 0x1FFFF;
    cbw_trb.control = (XHCI_TRB_TYPE_NORMAL << 10) | (1 << 0) | (1 << 5);

    ep_out->get_transfer_ring()->enqueue(&cbw_trb);
    hcd_->ring_doorbell(device_->get_slot_id(), ep_out->xhc_endpoint_num);
}

isize XhciMassStorageDriver::read(u64 lba, usize sector_count, void* buffer, usize buffer_size) {
    usize bytes = sector_count * sector_size_;
    if (!buffer || sector_count == 0 || buffer_size < bytes) return -EINVAL;

    kernel::MutexGuard guard(io_mutex_);

    usize pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    void* dma_phys = kernel::memory::request_pages(pages).ptr;
    if (!dma_phys) return -ENOMEM;

    auto& transfer = transfer_rw_;
    memset(&transfer, 0, sizeof(transfer));

    transfer.endpoint = bulk_in_endpoint_;  // IN-Endpoint
    transfer.data_buffer = dma_phys;        // DMA-Puffer
    transfer.data_length = bytes;
    transfer.is_input = true;

    // CBW aufbauen
    transfer.cbw.signature = 0x43425355;
    transfer.cbw.tag = current_tag_++;
    transfer.cbw.data_length = transfer.data_length;
    transfer.cbw.flags = 0x80;  // Data-In
    transfer.cbw.lun = 0;
    transfer.cbw.cb_length = 10;
    transfer.cbw.cb[0] = 0x28;  // READ(10)
    transfer.cbw.cb[2] = (lba >> 24) & 0xFF;
    transfer.cbw.cb[3] = (lba >> 16) & 0xFF;
    transfer.cbw.cb[4] = (lba >> 8) & 0xFF;
    transfer.cbw.cb[5] = lba & 0xFF;
    transfer.cbw.cb[7] = (sector_count >> 8) & 0xFF;
    transfer.cbw.cb[8] = sector_count & 0xFF;

    start_bulk_transfer(&transfer);

    while (!transfer.done) {
        kernel::scheduling::yield();
    }

    isize result = (transfer.status == 0) ? transfer.actual_length : -EIO;

    if (result > 0) memcpy(buffer, dma_phys, bytes);

    kernel::memory::free_pages(make_virt(dma_phys), pages);
    return result;
}

isize XhciMassStorageDriver::write(u64 lba, usize sector_count, const void* buffer, usize buffer_size) {
    usize bytes = sector_count * sector_size_;
    if (!buffer || sector_count == 0 || buffer_size < bytes) return -EINVAL;

    kernel::MutexGuard guard(io_mutex_);

    usize pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    void* dma_phys = kernel::memory::request_pages(pages).ptr;
    if (!dma_phys) return -ENOMEM;

    memcpy(dma_phys, buffer, bytes);

    auto& transfer = transfer_rw_;
    memset(&transfer, 0, sizeof(transfer));

    transfer.endpoint = bulk_out_endpoint_;  // OUT-Endpoint
    transfer.data_buffer = dma_phys;         // DMA-Puffer
    transfer.data_length = bytes;
    transfer.is_input = false;

    // CBW
    transfer.cbw.signature = 0x43425355;
    transfer.cbw.tag = current_tag_++;
    transfer.cbw.data_length = transfer.data_length;
    transfer.cbw.flags = 0x00;  // Data-Out
    transfer.cbw.lun = 0;
    transfer.cbw.cb_length = 10;
    transfer.cbw.cb[0] = 0x2A;  // WRITE(10)
    transfer.cbw.cb[2] = (lba >> 24) & 0xFF;
    transfer.cbw.cb[3] = (lba >> 16) & 0xFF;
    transfer.cbw.cb[4] = (lba >> 8) & 0xFF;
    transfer.cbw.cb[5] = lba & 0xFF;
    transfer.cbw.cb[7] = (sector_count >> 8) & 0xFF;
    transfer.cbw.cb[8] = sector_count & 0xFF;

    start_bulk_transfer(&transfer);

    while (!transfer.done) {
        kernel::scheduling::yield();
    }

    isize result = (transfer.status == 0) ? transfer.actual_length : -EIO;

    kernel::memory::free_pages(make_virt(dma_phys), pages);
    return result;
}

// TODO später scheduler FIFO queue beachten falls schon gelesen wird etc.

void XhciMassStorageDriver::detach() {
    if (kd_) {
        FilesystemDetector::emergency_detach_device(kd_->block);
    }

    current_transfer_ = nullptr;

    bulk_in_endpoint_ = nullptr;
    bulk_out_endpoint_ = nullptr;

    DevFs::unregister_device(kd_);
    DeviceManager::unregister_device(kd_);

    hcd_ = nullptr;
    device_ = nullptr;

    // Flags zurücksetzen
    init_status_ = -1;
    init_phase_ = InitPhase::Completed;
}

// TODO pls rework me, i am pretty shitty •`_´•