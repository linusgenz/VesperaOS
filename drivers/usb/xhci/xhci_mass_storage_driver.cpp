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

#include "xhci.h"
#include "xhci_mass_storage_driver.h"

#include "../../../filesystem/fat32/fat32.h"
#include "../../../filesystem/fat32/fat32_vfs_adapter.h"
#include "../../../kernel/devices/device_manager.h"


void xhciMassStorageDriver::on_startup(USB::xhciDriver* hcd, xhciDevice* dev) {
    this->hcd = hcd;
    this->device = dev;

    init_phase = InitPhase::TestUnitReady;
    initialize_device();
}


void xhciMassStorageDriver::on_event(USB::xhciDriver* hcd, xhciDevice* dev) {
    if (!current_transfer) return;

    switch (current_transfer->phase) {
        case MassStorageTransfer::Phase::SentCBW: {
            if (current_transfer->data_length > 0) {
                // Data phase: select endpoint according to direction
                auto* data_ep = current_transfer->is_input ? bulk_in_endpoint : bulk_out_endpoint;

                xhci_trb_t data_trb{};
                data_trb.parameter = xhci_get_physical_addr(current_transfer->data_buffer);
                data_trb.status    = current_transfer->data_length & 0x1FFFF;
                data_trb.control   = (XHCI_TRB_TYPE_NORMAL << 10) | (1 << 0) | (1 << 5);
                if (current_transfer->is_input) {
                    data_trb.control |= (1 << 16); // IN for data phase if necessary
                }
                data_ep->get_transfer_ring()->enqueue(&data_trb);
                hcd->ring_doorbell(dev->get_slot_id(), data_ep->xhc_endpoint_num);

                current_transfer->phase = MassStorageTransfer::Phase::DataPhase;
            } else {
                // No data phase → directly CSW (always IN)
                auto* ep_in = bulk_in_endpoint;

                xhci_trb_t csw_trb{};
                csw_trb.parameter = xhci_get_physical_addr(&current_transfer->csw);
                csw_trb.status    = sizeof(CSW) & 0x1FFFF;
                csw_trb.control   = (XHCI_TRB_TYPE_NORMAL << 10) | (1 << 0) | (1 << 5) | (1 << 16); // IN
                ep_in->get_transfer_ring()->enqueue(&csw_trb);
                hcd->ring_doorbell(dev->get_slot_id(), ep_in->xhc_endpoint_num);

                current_transfer->phase = MassStorageTransfer::Phase::ReceivedCSW;
            }
            break;
        }

        case MassStorageTransfer::Phase::DataPhase: {
            // Data phase complete → Request CSW (always IN)
            auto* ep_in = bulk_in_endpoint;

            xhci_trb_t csw_trb{};
            csw_trb.parameter = xhci_get_physical_addr(&current_transfer->csw);
            csw_trb.status    = sizeof(CSW) & 0x1FFFF;
            csw_trb.control   = (XHCI_TRB_TYPE_NORMAL << 10) | (1 << 0) | (1 << 5) | (1 << 16); // IN
            ep_in->get_transfer_ring()->enqueue(&csw_trb);
            hcd->ring_doorbell(dev->get_slot_id(), ep_in->xhc_endpoint_num);

            current_transfer->phase = MassStorageTransfer::Phase::ReceivedCSW;
            break;
        }

        case MassStorageTransfer::Phase::ReceivedCSW: {
            if (current_transfer->csw.status == 0) {
                current_transfer->phase = MassStorageTransfer::Phase::Completed;
            } else {
                current_transfer->phase = MassStorageTransfer::Phase::Error;
            }
            handle_completed_transfer();
            break;
        }

        default:
            break;
    }
}




void xhciMassStorageDriver::initialize_device() {
    // Bulk-Endpunkte suchen
    for (auto& ep : m_interface->endpoints) {
        if ((ep->usb_endpoint_attributes & 0x03) == 0x02) {
            if (ep->usb_endpoint_addr & 0x80)
                bulk_in_endpoint = ep;
            else
                bulk_out_endpoint = ep;
        }
    }

    if (!bulk_in_endpoint || !bulk_out_endpoint) {
        Log::Error("USB Mass Storage: Required bulk endpoints missing");
        return;
    }

    // initialization start: Test Unit Ready
    scsi_test_unit_ready();
}

void xhciMassStorageDriver::handle_completed_transfer() {
    if (!current_transfer) return;

    if (init_phase == InitPhase::TestUnitReady) {
        current_transfer = nullptr;
        init_phase = InitPhase::Inquiry;
        scsi_inquiry();
    }
    else if (init_phase == InitPhase::Inquiry) {
        current_transfer = nullptr;
        init_phase = InitPhase::ReadCapacity;
        scsi_read_capacity();
    }
    else if (init_phase == InitPhase::ReadCapacity) {
        auto* data = capacity_buffer;
        uint32_t last_lba   = (data[0]<<24) | (data[1]<<16) | (data[2]<<8) | data[3];
        uint32_t block_size = (data[4]<<24) | (data[5]<<16) | (data[6]<<8) | data[7];

        total_sectors = last_lba + 1;
        sector_size   = block_size;

        current_transfer = nullptr;
        init_phase = InitPhase::Completed;

        Log::PrintLn("USB Mass Storage initialized: %llu sectors, %u bytes/sector",
                     total_sectors, sector_size);
        kernel::DeviceManager::AddDevice(this);
    }
    else {
        // normal RW-Transfers
        current_transfer = nullptr;
    }
}

void xhciMassStorageDriver::scsi_test_unit_ready() {
    auto& transfer = transfer_test_unit_ready;
    memset(&transfer, 0, sizeof(transfer));

    transfer.endpoint    = bulk_out_endpoint; // OUT for CBW
    transfer.data_buffer = nullptr;
    transfer.data_length = 0;
    transfer.is_input    = false;

    transfer.cbw.signature    = 0x43425355;
    transfer.cbw.tag          = current_tag++;
    transfer.cbw.data_length  = 0;
    transfer.cbw.flags        = 0x00;
    transfer.cbw.lun          = 0;
    transfer.cbw.cb_length    = 6;
    transfer.cbw.cb[0]        = 0x00; // TEST UNIT READY

    start_bulk_transfer(&transfer);

    // The result is processed in on_event
}

void xhciMassStorageDriver::scsi_inquiry() {
    auto& transfer = transfer_inquiry;
    memset(&transfer, 0, sizeof(transfer));

    transfer.endpoint    = bulk_in_endpoint;
    transfer.data_buffer = inquiry_buffer;
    transfer.data_length = sizeof(inquiry_buffer);
    transfer.is_input    = true;

    transfer.cbw.signature = 0x43425355;
    transfer.cbw.tag       = current_tag++;
    transfer.cbw.data_length = sizeof(inquiry_buffer);
    transfer.cbw.flags     = 0x80; // Data-In
    transfer.cbw.cb_length = 6;
    transfer.cbw.cb[0]     = 0x12; // INQUIRY
    transfer.cbw.cb[4]     = sizeof(inquiry_buffer);

    start_bulk_transfer(&transfer);

    // The result is processed in on_event
}


void xhciMassStorageDriver::scsi_read_capacity() {
    auto& transfer = transfer_capacity;
    memset(&transfer, 0, sizeof(transfer));

    transfer.endpoint    = bulk_in_endpoint;
    transfer.data_buffer = capacity_buffer;
    transfer.data_length = sizeof(capacity_buffer);
    transfer.is_input    = true;

    transfer.cbw.signature    = 0x43425355;
    transfer.cbw.tag          = current_tag++;
    transfer.cbw.data_length  = sizeof(capacity_buffer);
    transfer.cbw.flags        = 0x80; // Data-In
    transfer.cbw.lun          = 0;
    transfer.cbw.cb_length    = 10;
    transfer.cbw.cb[0]        = 0x25; // READ CAPACITY(10)

    start_bulk_transfer(&transfer);

    // The result is processed in on_event
}


void xhciMassStorageDriver::start_bulk_transfer(MassStorageTransfer* transfer) {
    current_transfer = transfer;
    transfer->phase = MassStorageTransfer::Phase::SentCBW;

    auto* ep_out = bulk_out_endpoint;

    xhci_trb_t cbw_trb{};
    cbw_trb.parameter = xhci_get_physical_addr(&transfer->cbw);
    cbw_trb.status    = sizeof(CBW) & 0x1FFFF;
    cbw_trb.control   = (XHCI_TRB_TYPE_NORMAL << 10) | (1 << 0) | (1 << 5);

    ep_out->get_transfer_ring()->enqueue(&cbw_trb);
    hcd->ring_doorbell(device->get_slot_id(), ep_out->xhc_endpoint_num);
}


bool xhciMassStorageDriver::read(uint64_t lba, uint32_t sectorCount, void* buffer) {
    auto& transfer = transfer_rw;
    memset(&transfer, 0, sizeof(transfer));

    transfer.endpoint    = bulk_in_endpoint;       // IN-Endpoint
    transfer.data_buffer = buffer;                 // User-Buffer
    transfer.data_length = sectorCount * sector_size;
    transfer.is_input    = true;

    // CBW aufbauen
    transfer.cbw.signature   = 0x43425355;
    transfer.cbw.tag         = current_tag++;
    transfer.cbw.data_length = transfer.data_length;
    transfer.cbw.flags       = 0x80; // Data-In
    transfer.cbw.lun         = 0;
    transfer.cbw.cb_length   = 10;
    transfer.cbw.cb[0]       = 0x28; // READ(10)
    transfer.cbw.cb[2]       = (lba >> 24) & 0xFF;
    transfer.cbw.cb[3]       = (lba >> 16) & 0xFF;
    transfer.cbw.cb[4]       = (lba >> 8) & 0xFF;
    transfer.cbw.cb[5]       = lba & 0xFF;
    transfer.cbw.cb[7]       = (sectorCount >> 8) & 0xFF;
    transfer.cbw.cb[8]       = sectorCount & 0xFF;

    start_bulk_transfer(&transfer);

    return true;
}


bool xhciMassStorageDriver::write(uint64_t lba, uint32_t sectorCount, void* buffer) {
    auto& transfer = transfer_rw;
    memset(&transfer, 0, sizeof(transfer));

    transfer.endpoint    = bulk_out_endpoint;      // OUT-Endpoint
    transfer.data_buffer = buffer;                 // User-Buffer
    transfer.data_length = sectorCount * sector_size;
    transfer.is_input    = false;

    // CBW
    transfer.cbw.signature   = 0x43425355;
    transfer.cbw.tag         = current_tag++;
    transfer.cbw.data_length = transfer.data_length;
    transfer.cbw.flags       = 0x00; // Data-Out
    transfer.cbw.lun         = 0;
    transfer.cbw.cb_length   = 10;
    transfer.cbw.cb[0]       = 0x2A; // WRITE(10)
    transfer.cbw.cb[2]       = (lba >> 24) & 0xFF;
    transfer.cbw.cb[3]       = (lba >> 16) & 0xFF;
    transfer.cbw.cb[4]       = (lba >> 8) & 0xFF;
    transfer.cbw.cb[5]       = lba & 0xFF;
    transfer.cbw.cb[7]       = (sectorCount >> 8) & 0xFF;
    transfer.cbw.cb[8]       = sectorCount & 0xFF;

    start_bulk_transfer(&transfer);

    return true;
}

