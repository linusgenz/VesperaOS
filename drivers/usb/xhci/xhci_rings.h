#ifndef XHCI_RINGS_H
#define XHCI_RINGS_H

#include <klib/vector.h>

#include "xhci_regs.h"
#include "xhci_trb.h"

class XhciCommandRing {
   public:
    explicit XhciCommandRing(usize max_trbs);

    [[nodiscard]] xhci_trb_t* get_virtual_base() const {
        return trbs_;
    }
    [[nodiscard]] uptr get_physical_base() const {
        return physical_base_;
    }
    [[nodiscard]] u8 get_cycle_bit() const {
        return rcs_bit_;
    }

    void enqueue(xhci_trb_t* trb);

   private:
    Spinlock lock_{};

    usize max_trb_count_;     // Number of valid TRBs in the ring including the LINK_TRB
    usize enqueue_ptr_;       // Index in the ring where to enqueue next TRB
    xhci_trb_t* trbs_;         // Base address of the ring buffer
    uptr physical_base_;  // Physical base of the ring
    u8 rcs_bit_;          // Ring cycle state
};

/*
// xHci Spec Section 6.5 Event Ring Segment Table Figure 6-40: Event Ring Segment Table Entry

Note: The Ring Segment Size may be set to any value from 16 to 4096, however
software shall allocate a buffer for the Event Ring Segment that rounds up its
size to the nearest 64B boundary to allow full cache-line accesses.
*/
struct XHCI_ERST_ENTRY {
    u64 ring_segment_base_address;  // Base address of the Event Ring segment
    u32 ring_segment_size;          // Size of the Event Ring segment (only low 16 bits are used)
    u32 rsvd;
} __attribute__((packed));

class XhciEventRing {
   public:
    XhciEventRing(usize max_trbs, volatile XHCI_INTERRUPTER_REGISTERS* interrupter);

    [[nodiscard]] xhci_trb_t* get_virtual_base() const {
        return trbs_;
    }
    [[nodiscard]] uptr get_physical_base() const {
        return physical_base_;
    }
    [[nodiscard]] u8 get_cycle_bit() const {
        return rcs_bit_;
    }

    [[nodiscard]] bool has_unprocessed_events() const;
    void dequeue_events(Vector<xhci_trb_t*>& trbs);

    [[nodiscard]] u64 get_current_dequeue_physical() const;

   private:
    Spinlock lock_{};

    volatile XHCI_INTERRUPTER_REGISTERS* interrupter_regs_;

    usize segment_trb_count_;  // Max TRBs allowed on the segment

    xhci_trb_t* trbs_;  // Primary segment ring base
    uptr physical_base_;

    XHCI_ERST_ENTRY* segment_table_;  // Event ring segment table base

    u64 dequeue_ptr_;  // Event ring dequeue pointer
    u8 rcs_bit_;       // Ring cycle state

   private:
    void update_erdp() const;
    xhci_trb_t* dequeue_trb();
};

class XhciTransferRing {
   public:
    static XhciTransferRing* allocate(u8 slot_id);

    XhciTransferRing(usize max_trbs, u8 doorbell_id);

    ~XhciTransferRing() = default;

    [[nodiscard]] xhci_trb_t* get_virtual_base() const {
        return trbs_;
    }
    [[nodiscard]] uptr get_physical_base() const {
        return physical_base_;
    }
    [[nodiscard]] u8 get_cycle_bit() const {
        return rcs_bit_;
    }
    [[nodiscard]] u8 get_doorbell_id() const {
        return doorbell_id_;
    }

    [[nodiscard]] uptr get_physical_dequeue_pointer_base() const;

    void enqueue(xhci_trb_t* trb);

   private:
    Spinlock lock_{};

    usize max_trb_count_;  // Number of valid TRBs in the ring including the LINK_TRB
    usize dequeue_ptr_;    // Transfer ring consumer dequeue pointer
    usize enqueue_ptr_;    // Transfer ring producer enqueue pointer
    xhci_trb_t* trbs_;      // Base address of the ring buffer
    uptr physical_base_;
    u8 rcs_bit_;      // Dequeue cycle state
    u8 doorbell_id_;  // ID of the doorbell associated with the ring
};

#endif  // XHCI_RINGS_H
