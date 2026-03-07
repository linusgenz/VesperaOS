#include "xhci_rings.h"

#include <vespera/log.h>
#include <klib/vector.h>

XhciCommandRing::XhciCommandRing(size_t max_trbs) {
    lock_.init("xhci_command_ring_lock");

    max_trb_count_ = max_trbs;
    rcs_bit_ = XHCI_CRCR_RING_CYCLE_STATE;
    enqueue_ptr_ = 0;

    const uint64_t ring_size = max_trbs * sizeof(xhci_trb_t);

    trbs_ = static_cast<xhci_trb_t*>(
        alloc_xhci_memory(ring_size, XHCI_COMMAND_RING_SEGMENTS_ALIGNMENT, XHCI_COMMAND_RING_SEGMENTS_BOUNDARY)
    );

    physical_base_ = xhci_get_physical_addr(trbs_);

    // Set the last TRB as a link TRB to point back to the first TRB
    trbs_[max_trb_count_ - 1].parameter = physical_base_;
    trbs_[max_trb_count_ - 1].control = (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_LINK_TRB_TC_BIT | rcs_bit_;
}

void XhciCommandRing::enqueue(xhci_trb_t* trb) {
    SpinlockGuardIrq guard(lock_);

    // Adjust the TRB's cycle bit to the current RCS
    trb->cycle_bit = rcs_bit_;

    // Insert the TRB into the ring
    trbs_[enqueue_ptr_] = *trb;

    if (++enqueue_ptr_ == max_trb_count_ - 1) {
        trbs_[max_trb_count_ - 1].control =
            (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_LINK_TRB_TC_BIT | rcs_bit_;

        enqueue_ptr_ = 0;
        rcs_bit_ = !rcs_bit_;
    }
}

XhciEventRing::XhciEventRing(size_t max_trbs, volatile XHCI_INTERRUPTER_REGISTERS* interrupter)
    : interrupter_regs_(interrupter)
    , segment_trb_count_(max_trbs)
    , dequeue_ptr_(0)
    , rcs_bit_(XHCI_CRCR_RING_CYCLE_STATE) {
    lock_.init("xhci_event_ring_lock");

    constexpr uint64_t segment_count = 1;  // TODO use more seg's later

    const uint64_t segment_size = max_trbs * sizeof(xhci_trb_t);
    constexpr uint64_t segment_table_size = segment_count * sizeof(XHCI_ERST_ENTRY);

    trbs_ = static_cast<xhci_trb_t*>(
        alloc_xhci_memory(segment_size, XHCI_EVENT_RING_SEGMENTS_ALIGNMENT, XHCI_EVENT_RING_SEGMENTS_BOUNDARY)
    );

    // Store the physical DMA base
    physical_base_ = xhci_get_physical_addr(trbs_);

    // Create the event ring segment table
    segment_table_ = static_cast<XHCI_ERST_ENTRY*>(alloc_xhci_memory(
        segment_table_size, XHCI_EVENT_RING_SEGMENT_TABLE_ALIGNMENT, XHCI_EVENT_RING_SEGMENT_TABLE_BOUNDARY
    ));

    // Construct the segment table entry
    XHCI_ERST_ENTRY entry{};
    entry.ring_segment_base_address = physical_base_;
    entry.ring_segment_size = segment_trb_count_;
    entry.rsvd = 0;

    // Insert the constructed segment into the table
    segment_table_[0] = entry;

    // Configure the Event Ring Segment Table Size (ERSTSZ) register
    interrupter_regs_->erstsz = 1;

    update_erdp();

    // Write to ERSTBA register
    interrupter_regs_->erstba = xhci_get_physical_addr(segment_table_);
}

bool XhciEventRing::has_unprocessed_events() const {
    return (trbs_[dequeue_ptr_].cycle_bit == rcs_bit_);
}

/*
void xhciEventRing::dequeue_events(Vector<xhci_trb_t*>& trbs) {
    // Process each event TRB
    while (has_unprocessed_events()) {
        xhci_trb_t* trb = _dequeue_trb();
        if (!trb) {
            break;
        }

        trbs.push_back(trb);
    }

    // Update the ERDP register
    _update_erdp();

    // Clear the EHB (Event Handler Busy) bit

    uint64_t dequeue_address = physical_base_ + (dequeue_ptr_ * sizeof(xhci_trb_t));
    dequeue_address |= XHCI_ERDP_EHB; // Event Handler Busy Bit setzen
    interrupter_regs_->erdp = dequeue_address;
}*/

void XhciEventRing::dequeue_events(Vector<xhci_trb_t*>& trbs) {
    SpinlockGuardIrq guard(lock_);

    while (has_unprocessed_events()) {
        xhci_trb_t* trb = dequeue_trb();
        if (!trb) {
            break;
        }

        trbs.push_back(trb);
    }

    // Update the ERDP register
    update_erdp();

    // Clear the EHB (Event Handler Busy) bit
    uint64_t dequeue_address = physical_base_ + (dequeue_ptr_ * sizeof(xhci_trb_t));
    dequeue_address |= XHCI_ERDP_EHB;  // Event Handler Busy Bit setzen
    interrupter_regs_->erdp = dequeue_address;
}

void XhciEventRing::update_erdp() const {
    uint64_t dequeue_address = physical_base_ + (dequeue_ptr_ * sizeof(xhci_trb_t));
    interrupter_regs_->erdp = dequeue_address;
}

xhci_trb_t* XhciEventRing::dequeue_trb() {
    if (trbs_[dequeue_ptr_].cycle_bit != rcs_bit_) {
        Log::print_ln("Event Ring attempted to dequeue an invalid TRB, returning nullptr!\n");
        return nullptr;
    }

    // Get the resulting TRB
    xhci_trb_t* ret = &trbs_[dequeue_ptr_];

    // Advance and possibly wrap the dequeue pointer if needed
    if (++dequeue_ptr_ == segment_trb_count_) {
        dequeue_ptr_ = 0;
        rcs_bit_ = !rcs_bit_;
    }

    return ret;
}

uint64_t XhciEventRing::get_current_dequeue_physical() const {
    return dequeue_ptr_;
}

XhciTransferRing* XhciTransferRing::allocate(uint8_t slot_id) {
    return new XhciTransferRing(XHCI_TRANSFER_RING_TRB_COUNT, slot_id);
}

XhciTransferRing::XhciTransferRing(size_t max_trbs, uint8_t doorbell_id)
    : max_trb_count_(max_trbs)
    , dequeue_ptr_(0)
    , enqueue_ptr_(0)
    , rcs_bit_(1)
    , doorbell_id_(doorbell_id) {
    lock_.init("xhci_transfer_ring_lock");

    const uint64_t ring_size = max_trbs * sizeof(xhci_trb_t);

    // Create the transfer ring memory block
    trbs_ = static_cast<xhci_trb_t*>(
        alloc_xhci_memory(ring_size, XHCI_TRANSFER_RING_SEGMENTS_ALIGNMENT, XHCI_TRANSFER_RING_SEGMENTS_BOUNDARY)
    );
    physical_base_ =xhci_get_physical_addr(trbs_);

    // Set the last TRB as a link TRB to point back to the first TRB
    trbs_[max_trb_count_ - 1].parameter = physical_base_;
    trbs_[max_trb_count_ - 1].control = (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_LINK_TRB_TC_BIT | rcs_bit_;
}

uintptr_t XhciTransferRing::get_physical_dequeue_pointer_base() const {
    return xhci_get_physical_addr(&trbs_[enqueue_ptr_]);
}

void XhciTransferRing::enqueue(xhci_trb_t* trb) {
    SpinlockGuardIrq guard(lock_);

    // Adjust the TRB's cycle bit to the current DCS
    trb->cycle_bit = rcs_bit_;

    // Insert the TRB into the ring
    trbs_[enqueue_ptr_] = *trb;

    // Advance and possibly wrap the enqueue pointer if needed.
    // maxTrbCount - 1 accounts for the LINK_TRB.
    if (++enqueue_ptr_ == max_trb_count_ - 1) {
        // Only now update the Link TRB, syncing its cycle bit and setting the TC flag.
        trbs_[max_trb_count_ - 1].control =
            (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_LINK_TRB_TC_BIT | rcs_bit_;

        enqueue_ptr_ = 0;
        rcs_bit_ = !rcs_bit_;
    }
}