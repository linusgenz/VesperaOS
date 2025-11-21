#include "xhci_rings.h"
#include "../../../include/log.h"
#include "../../../include/vector.h"

xhciCommandRing::xhciCommandRing(size_t max_trbs) {
    m_lock.init();
    lock_debug_register(&m_lock, "xhci_command_ring_lock");

    m_max_trb_count = max_trbs;
    m_rcs_bit = XHCI_CRCR_RING_CYCLE_STATE;
    m_enqueue_ptr = 0;

    const uint64_t ring_size = max_trbs * sizeof(xhci_trb_t);

    m_trbs = (xhci_trb_t*)alloc_xhci_memory(
        ring_size,
        XHCI_COMMAND_RING_SEGMENTS_ALIGNMENT,
        XHCI_COMMAND_RING_SEGMENTS_BOUNDARY
    );

    m_physical_base = xhci_get_physical_addr(m_trbs);

    // Set the last TRB as a link TRB to point back to the first TRB
    m_trbs[m_max_trb_count - 1].parameter = m_physical_base;
    m_trbs[m_max_trb_count - 1].control =
        (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_LINK_TRB_TC_BIT | m_rcs_bit;
}

void xhciCommandRing::enqueue(xhci_trb_t* trb) {
    spinlock_guard_irq guard(m_lock);

    // Adjust the TRB's cycle bit to the current RCS
    trb->cycle_bit = m_rcs_bit;

    // Insert the TRB into the ring
    m_trbs[m_enqueue_ptr] = *trb;

    if (++m_enqueue_ptr == m_max_trb_count - 1) {
        m_trbs[m_max_trb_count - 1].control =
            (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_LINK_TRB_TC_BIT | m_rcs_bit;

        m_enqueue_ptr = 0;
        m_rcs_bit = !m_rcs_bit;
    }
}

xhciEventRing::xhciEventRing(
    size_t max_trbs,
    volatile xhci_interrupter_registers* interrupter
) {
    m_lock.init();
    lock_debug_register(&m_lock, "xhci_event_ring_lock");

    m_interrupter_regs = interrupter;
    m_segment_trb_count = max_trbs;
    m_rcs_bit = XHCI_CRCR_RING_CYCLE_STATE;
    m_dequeue_ptr = 0;

    const uint64_t segment_count = 1; // TODO use more seg's later

    const uint64_t segment_size = max_trbs * sizeof(xhci_trb_t);
    const uint64_t segment_table_size = segment_count * sizeof(xhci_erst_entry);

    m_trbs = (xhci_trb_t*)alloc_xhci_memory(
        segment_size,
        XHCI_EVENT_RING_SEGMENTS_ALIGNMENT,
        XHCI_EVENT_RING_SEGMENTS_BOUNDARY
    );

    // Store the physical DMA base
    m_physical_base = xhci_get_physical_addr(m_trbs);

    // Create the event ring segment table
    m_segment_table = (xhci_erst_entry*)alloc_xhci_memory(
        segment_table_size,
        XHCI_EVENT_RING_SEGMENT_TABLE_ALIGNMENT,
        XHCI_EVENT_RING_SEGMENT_TABLE_BOUNDARY
    );

    // Construct the segment table entry
    xhci_erst_entry entry;
    entry.ring_segment_base_address = m_physical_base;
    entry.ring_segment_size = m_segment_trb_count;
    entry.rsvd = 0;

    // Insert the constructed segment into the table
    m_segment_table[0] = entry;

    // Configure the Event Ring Segment Table Size (ERSTSZ) register
    m_interrupter_regs->erstsz = 1;

    _update_erdp();

    // Write to ERSTBA register
    m_interrupter_regs->erstba = xhci_get_physical_addr(m_segment_table);
}

bool xhciEventRing::has_unprocessed_events() {
    return (m_trbs[m_dequeue_ptr].cycle_bit == m_rcs_bit);
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

    uint64_t dequeue_address = m_physical_base + (m_dequeue_ptr * sizeof(xhci_trb_t));
    dequeue_address |= XHCI_ERDP_EHB; // Event Handler Busy Bit setzen
    m_interrupter_regs->erdp = dequeue_address;
}*/

void xhciEventRing::dequeue_events(Vector<xhci_trb_t*>& trbs) {
    spinlock_guard_irq guard(m_lock);

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
    uint64_t dequeue_address = m_physical_base + (m_dequeue_ptr * sizeof(xhci_trb_t));
    dequeue_address |= XHCI_ERDP_EHB; // Event Handler Busy Bit setzen
    m_interrupter_regs->erdp = dequeue_address;
}

void xhciEventRing::flush_unprocessed_events() {
  //  Vector<xhci_trb_t*> events;
  //  dequeue_events(events, TODO, TODO);
  //  events.clear();
}

void xhciEventRing::_update_erdp() {
    uint64_t dequeue_address = m_physical_base + (m_dequeue_ptr * sizeof(xhci_trb_t));
    m_interrupter_regs->erdp = dequeue_address;
}

xhci_trb_t* xhciEventRing::_dequeue_trb() {
    if (m_trbs[m_dequeue_ptr].cycle_bit != m_rcs_bit) {
        Log::PrintLn("Event Ring attempted to dequeue an invalid TRB, returning nullptr!\n");
        return nullptr;
    }

    // Get the resulting TRB
    xhci_trb_t* ret = &m_trbs[m_dequeue_ptr];

    // Advance and possibly wrap the dequeue pointer if needed
    if (++m_dequeue_ptr == m_segment_trb_count) {
        m_dequeue_ptr = 0;
        m_rcs_bit = !m_rcs_bit;
    }

    return ret;
}

uint64_t xhciEventRing::get_current_dequeue_physical() {
    return m_dequeue_ptr;
}


xhciTransferRing *xhciTransferRing::allocate(uint8_t slot_id) {
    return new xhciTransferRing(XHCI_TRANSFER_RING_TRB_COUNT, slot_id);
}

xhciTransferRing::xhciTransferRing(size_t max_trbs, uint8_t doorbell_id) {
    m_lock.init();
    lock_debug_register(&m_lock, "xhci_transfer_ring_lock");

    m_max_trb_count = max_trbs;
    m_rcs_bit = 1;
    m_dequeue_ptr = 0;
    m_enqueue_ptr = 0;
    m_doorbell_id = doorbell_id;

    const uint64_t ring_size = max_trbs * sizeof(xhci_trb_t);

    // Create the transfer ring memory block
    m_trbs = (xhci_trb_t*)alloc_xhci_memory(
        ring_size,
        XHCI_TRANSFER_RING_SEGMENTS_ALIGNMENT,
        XHCI_TRANSFER_RING_SEGMENTS_BOUNDARY
    );

    m_physical_base = xhci_get_physical_addr(m_trbs);

    // Set the last TRB as a link TRB to point back to the first TRB
    m_trbs[m_max_trb_count - 1].parameter = m_physical_base;
    m_trbs[m_max_trb_count - 1].control = (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_LINK_TRB_TC_BIT | m_rcs_bit;
}

uintptr_t xhciTransferRing::get_physical_dequeue_pointer_base() const {
    return xhci_get_physical_addr(&m_trbs[m_enqueue_ptr]);
}

void xhciTransferRing::enqueue(xhci_trb_t* trb) {
    spinlock_guard_irq guard(m_lock);

    // Adjust the TRB's cycle bit to the current DCS
    trb->cycle_bit = m_rcs_bit;

    // Insert the TRB into the ring
    m_trbs[m_enqueue_ptr] = *trb;

    // Advance and possibly wrap the enqueue pointer if needed.
    // maxTrbCount - 1 accounts for the LINK_TRB.
    if (++m_enqueue_ptr == m_max_trb_count - 1) {
        // Only now update the Link TRB, syncing its cycle bit and setting the TC flag.
        m_trbs[m_max_trb_count - 1].control =
            (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_LINK_TRB_TC_BIT | m_rcs_bit;

        m_enqueue_ptr = 0;
        m_rcs_bit = !m_rcs_bit;
    }
}