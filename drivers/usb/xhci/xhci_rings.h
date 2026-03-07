#ifndef XHCI_RINGS_H
#define XHCI_RINGS_H

#include <klib/vector.h>

#include "xhci_regs.h"
#include "xhci_trb.h"

class XhciCommandRing {
   public:
    explicit XhciCommandRing(size_t max_trbs);

    [[nodiscard]] xhci_trb_t* get_virtual_base() const {
        return trbs_;
    }
    [[nodiscard]] uintptr_t get_physical_base() const {
        return physical_base_;
    }
    [[nodiscard]] uint8_t get_cycle_bit() const {
        return rcs_bit_;
    }

    void enqueue(xhci_trb_t* trb);

   private:
    Spinlock lock_{};

    size_t max_trb_count_;     // Number of valid TRBs in the ring including the LINK_TRB
    size_t enqueue_ptr_;       // Index in the ring where to enqueue next TRB
    xhci_trb_t* trbs_;         // Base address of the ring buffer
    uintptr_t physical_base_;  // Physical base of the ring
    uint8_t rcs_bit_;          // Ring cycle state
};

/*
// xHci Spec Section 6.5 Event Ring Segment Table Figure 6-40: Event Ring Segment Table Entry

Note: The Ring Segment Size may be set to any value from 16 to 4096, however
software shall allocate a buffer for the Event Ring Segment that rounds up its
size to the nearest 64B boundary to allow full cache-line accesses.
*/
struct XHCI_ERST_ENTRY {
    uint64_t ring_segment_base_address;  // Base address of the Event Ring segment
    uint32_t ring_segment_size;          // Size of the Event Ring segment (only low 16 bits are used)
    uint32_t rsvd;
} __attribute__((packed));

class XhciEventRing {
   public:
    XhciEventRing(size_t max_trbs, volatile XHCI_INTERRUPTER_REGISTERS* interrupter);

    [[nodiscard]] xhci_trb_t* get_virtual_base() const {
        return trbs_;
    }
    [[nodiscard]] uintptr_t get_physical_base() const {
        return physical_base_;
    }
    [[nodiscard]] uint8_t get_cycle_bit() const {
        return rcs_bit_;
    }

    [[nodiscard]] bool has_unprocessed_events() const;
    void dequeue_events(Vector<xhci_trb_t*>& trbs);

    [[nodiscard]] uint64_t get_current_dequeue_physical() const;

   private:
    Spinlock lock_{};

    volatile XHCI_INTERRUPTER_REGISTERS* interrupter_regs_;

    size_t segment_trb_count_;  // Max TRBs allowed on the segment

    xhci_trb_t* trbs_;  // Primary segment ring base
    uintptr_t physical_base_;

    XHCI_ERST_ENTRY* segment_table_;  // Event ring segment table base

    uint64_t dequeue_ptr_;  // Event ring dequeue pointer
    uint8_t rcs_bit_;       // Ring cycle state

   private:
    void update_erdp() const;
    xhci_trb_t* dequeue_trb();
};

class XhciTransferRing {
   public:
    static XhciTransferRing* allocate(uint8_t slot_id);

    XhciTransferRing(size_t max_trbs, uint8_t doorbell_id);

    ~XhciTransferRing() = default;

    [[nodiscard]] xhci_trb_t* get_virtual_base() const {
        return trbs_;
    }
    [[nodiscard]] uintptr_t get_physical_base() const {
        return physical_base_;
    }
    [[nodiscard]] uint8_t get_cycle_bit() const {
        return rcs_bit_;
    }
    [[nodiscard]] uint8_t get_doorbell_id() const {
        return doorbell_id_;
    }

    [[nodiscard]] uintptr_t get_physical_dequeue_pointer_base() const;

    void enqueue(xhci_trb_t* trb);

   private:
    Spinlock lock_{};

    size_t max_trb_count_;  // Number of valid TRBs in the ring including the LINK_TRB
    size_t dequeue_ptr_;    // Transfer ring consumer dequeue pointer
    size_t enqueue_ptr_;    // Transfer ring producer enqueue pointer
    xhci_trb_t* trbs_;      // Base address of the ring buffer
    uintptr_t physical_base_;
    uint8_t rcs_bit_;      // Dequeue cycle state
    uint8_t doorbell_id_;  // ID of the doorbell associated with the ring
};

#endif  // XHCI_RINGS_H
