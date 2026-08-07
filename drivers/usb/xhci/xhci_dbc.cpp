// xhci_dbc.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 21.05.26.
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

#include "xhci_dbc.h"

#include <klib/string.h>
#include <vespera/log.h>
#include <vespera/time.h>
#include <vespera/types.h>

#include "xhci_common.h"
#include "xhci_dbc_ctx.h"
#include "xhci_dbc_regs.h"
#include "xhci_mem.h"
#include "xhci_rings.h"
#include "xhci_trb.h"
#include "drivers/mmio_post_write.h"

namespace usb {

    // ============================================================================
    //  Module-local constants
    // ============================================================================

    /** @brief Number of TRBs in the DbC event ring.  One segment only. */
    constexpr usize DBC_EVENT_RING_TRB_COUNT = 256;

    /**
     * @brief Maximum bytes per single IN or OUT DMA transfer.
     *
     * Both bounce buffers are allocated to this size.  Callers that need to
     * transfer more data must chunk their calls.
     */
    constexpr usize DBC_BOUNCE_BUF_SIZE = 4096;

    constexpr u64 DBC_TRANSFER_TIMEOUT_NS = 5'000'000'000ULL;

    /** @brief idVendor presented in the USB Device Descriptor (Linux Foundation). */
    constexpr u16 DBC_VENDOR_ID = 0x1D6Bu;
    /** @brief idProduct presented in the USB Device Descriptor. */
    constexpr u16 DBC_PRODUCT_ID = 0x0010u;
    /** @brief bcdDevice presented in the USB Device Descriptor. */
    constexpr u16 DBC_DEVICE_REVISION = 0x0100u;

    // ============================================================================
    //  Internal data structures
    // ============================================================================

    /**
     * @brief xHCI ERST entry used by the DbC event ring.
     *
     * Identical binary layout to the main xHCI ERST entry (§6.5).
     * Defined locally to avoid pulling in the full xhci_regs.h dependency.
     */
    struct __attribute__((packed)) DBC_ERST_ENTRY {
        u64 ring_segment_base_address;  ///< Physical base of the event ring segment.
        u32 ring_segment_size;          ///< Number of TRBs in the segment.
        u32 rsvd;                       ///< RsvdP — must be zero.
    };
    static_assert(sizeof(DBC_ERST_ENTRY) == 16);

    /** @brief USB language ID string descriptor (String 0). §9.6.7 */
    struct __attribute__((packed)) dbc_langid_desc {
        u8 b_length;           ///< = 4
        u8 b_descriptor_type;  ///< = USB_DT_STRING (0x03)
        u16 lang_id;           ///< Language ID, e.g. DBC_LANGID_EN_US (0x0409)
    };
    static_assert(sizeof(dbc_langid_desc) == 4);

    // ============================================================================
    //  XhciDbcEventRing
    // ============================================================================

    /**
     * @brief Lightweight poll-mode event ring for the xHCI Debug Capability.
     *
     * Allocates one event ring segment and its ERST, then programs DCERSTSZ,
     * DCERDP, and DCERSTBA in DBC_REGS directly.  Unlike XhciEventRing it does
     * not use XHCI_INTERRUPTER_REGISTERS and fires no interrupts.
     *
     * Typical usage:
     * @code
     * XhciDbcEventRing er(regs, 256);
     * xhci_trb_t event{};
     * while (!er.poll_event(&event)) { __asm__ volatile("pause"); }
     * @endcode
     *
     * @note Construct before setting DCCTRL.DCE = 1.
     * @note Not thread-safe — all polling must occur from a single context.
     */
    class XhciDbcEventRing {
       public:
        /**
         * @brief Allocates DMA memory and programs the DbC event ring registers.
         *
         * @param regs      Pointer to the DbC MMIO register block.
         * @param trb_count Number of TRBs in the single ring segment.
         */
        explicit XhciDbcEventRing(volatile DBC_REGS* regs, usize trb_count);

        /**
         * @brief Attempts to dequeue one event TRB from the ring.
         *
         * Checks the cycle bit of the next TRB against the current cycle state.
         * If a valid event is present it is copied to @p out, the dequeue pointer
         * is advanced, and DCERDP is updated.
         *
         * @param out  Caller buffer; overwritten with the event TRB on success.
         *
         * @return true  if an event was dequeued into @p out.
         * @return false if the ring is empty (no new event available).
         */
        [[nodiscard]] bool poll_event(xhci_trb_t* out);

       private:
        /** @brief Writes the current dequeue pointer back to DCERDP. */
        void update_dcerdp() const;

        volatile DBC_REGS* regs_;
        xhci_trb_t* trbs_;      ///< Virtual address of the TRB array.
        DBC_ERST_ENTRY* erst_;  ///< Virtual address of the ERST.
        u64 phys_base_;         ///< Physical base of the TRB array.
        usize trb_count_;       ///< Total TRBs in the segment.
        usize dequeue_ptr_;     ///< Index of the next TRB to consume.
        u8 ccs_;                ///< Current cycle state (toggles on wrap-around).
    };

    XhciDbcEventRing::XhciDbcEventRing(volatile DBC_REGS* const regs, const usize trb_count)
        : regs_(regs)
        , trbs_(nullptr)
        , erst_(nullptr)
        , phys_base_(0)
        , trb_count_(trb_count)
        , dequeue_ptr_(0)
        , ccs_(1) {
        const usize ring_bytes = trb_count * sizeof(xhci_trb_t);

        trbs_ = static_cast<xhci_trb_t*>(
            alloc_xhci_memory(ring_bytes, XHCI_EVENT_RING_SEGMENTS_ALIGNMENT, XHCI_EVENT_RING_SEGMENTS_BOUNDARY)
        );
        phys_base_ = xhci_get_physical_addr(trbs_);

        erst_ = static_cast<DBC_ERST_ENTRY*>(alloc_xhci_memory(
            sizeof(DBC_ERST_ENTRY), XHCI_EVENT_RING_SEGMENT_TABLE_ALIGNMENT, XHCI_EVENT_RING_SEGMENT_TABLE_BOUNDARY
        ));
        erst_->ring_segment_base_address = phys_base_;
        erst_->ring_segment_size = static_cast<u32>(trb_count);
        erst_->rsvd = 0;

        // Per spec §7.6.4: DCERSTSZ must be written before DCERSTBA.
        regs_->dcerstsz.erst_size = 1;

        // DCERDP — dequeue starts at ring base; DESI = 0 (single segment).
        DBC_DCERDP_REGISTER reg{};
        reg.raw = 0;

        reg.desi = 0;
        reg.deq_ptr = phys_base_ >> 4;

        regs_->dcerdp.raw = reg.raw;

        // DCERSTBA — physical address of ERST, bits 3:0 RsvdP.
        regs_->dcerstba = xhci_get_physical_addr(erst_) & DBC_DCERSTBA_ADDR_MASK;
    }

    bool XhciDbcEventRing::poll_event(xhci_trb_t* const out) {
        // A TRB is valid when its cycle bit matches the current cycle state.
        if (trbs_[dequeue_ptr_].cycle_bit != ccs_) {
            return false;
        }

        *out = trbs_[dequeue_ptr_];

        if (++dequeue_ptr_ == trb_count_) {
            dequeue_ptr_ = 0;
            ccs_ = static_cast<u8>(!ccs_);
        }

        update_dcerdp();
        return true;
    }

    void XhciDbcEventRing::update_dcerdp() const {
        const u64 phys = phys_base_ + dequeue_ptr_ * sizeof(xhci_trb_t);

        DBC_DCERDP_REGISTER reg{};
        reg.raw = 0;

        reg.desi = 0;  // always 0 for single segment
        reg.deq_ptr = phys >> 4;

        regs_->dcerdp.raw = reg.raw;
        MMIO_POST_WRITE64_PTR(&regs_->dcerdp);
    }

    // ============================================================================
    //  XhciDbcPort — public interface
    // ============================================================================

    bool XhciDbcPort::init(volatile DBC_REGS* const regs) {
        regs_ = regs;

        dbc_ctx_ = alloc_xhci_memory(sizeof(DBC_CONTEXT), XHCI_DEVICE_CONTEXT_ALIGNMENT, PAGE_SIZE);
        if (!dbc_ctx_) {
            Log::print_ln("xhci_dbc: failed to allocate DBC_CONTEXT\n");
            return false;
        }
        memset(dbc_ctx_, 0, sizeof(DBC_CONTEXT));

        auto* const ctx = static_cast<DBC_CONTEXT*>(dbc_ctx_);

        // String 0: language ID list (English US)
        // String 1: manufacturer   "VesperaOS"
        // String 2: product        "xHCI DbC"
        // String 3: serial         — not used; serial_length stays 0

        static constexpr char k_manufacturer[] = "VesperaOS";
        static constexpr char k_product[] = "xHCI DbC";

        constexpr usize k_mfr_len = sizeof(k_manufacturer) - 1u;
        constexpr usize k_prod_len = sizeof(k_product) - 1u;

        auto* const str0 = new USB_STRING_LANGUAGE_DESCRIPTOR{};
        str0->header.b_length = 4u;
        str0->header.b_descriptor_type = USB_DESCRIPTOR_STRING;
        str0->lang_ids[0] = DBC_LANGID_EN_US;

        auto* const mfr = new USB_STRING_DESCRIPTOR{};
        mfr->header.b_length = static_cast<u8>(2u + k_mfr_len * 2u);
        mfr->header.b_descriptor_type = USB_DESCRIPTOR_STRING;
        for (usize i = 0; i < k_mfr_len; ++i) {
            mfr->unicode_string[i] = static_cast<u16>(static_cast<unsigned char>(k_manufacturer[i]));
        }

        auto* const prod = new USB_STRING_DESCRIPTOR{};
        prod->header.b_length = static_cast<u8>(2u + k_prod_len * 2u);
        prod->header.b_descriptor_type = USB_DESCRIPTOR_STRING;
        for (usize i = 0; i < k_prod_len; ++i) {
            prod->unicode_string[i] = static_cast<u16>(static_cast<unsigned char>(k_product[i]));
        }

        ctx->info.string0_ptr = xhci_get_physical_addr(str0) & DBC_IC_STR_PTR_MASK;
        ctx->info.manufacturer_ptr = xhci_get_physical_addr(mfr) & DBC_IC_STR_PTR_MASK;
        ctx->info.product_ptr = xhci_get_physical_addr(prod) & DBC_IC_STR_PTR_MASK;
        ctx->info.serial_number_ptr = 0;

        ctx->info.string0_length = str0->header.b_length;
        ctx->info.manufacturer_length = mfr->header.b_length;
        ctx->info.product_length = prod->header.b_length;
        ctx->info.serial_length = 0;

        // doorbell_id 0 is unused — DbC rings its doorbells directly via DCDB.
        out_ring_ = XhciTransferRing::allocate(0);
        in_ring_ = XhciTransferRing::allocate(0);

        if (!out_ring_ || !in_ring_) {
            Log::print_ln("xhci_dbc: failed to allocate transfer rings\n");
            return false;
        }

        // Fill DbC Endpoint Contexts

        u8 max_burst = regs_->dcctrl.max_burst ? regs_->dcctrl.max_burst : 8;

        auto& ep = ctx->ep_out;

        // DW0
        ep.endpoint_state = 0;
        ep.mult = 0;
        ep.max_primary_streams = 0;
        ep.linear_stream_array = 0;
        ep.interval = 0;
        ep.max_esit_payload_hi = 0;

        // DW1
        ep.error_count = 3;
        ep.endpoint_type = DBC_EP_TYPE_BULK_OUT;
        ep.host_initiate_disable = 0;
        ep.max_burst_size = max_burst;
        ep.max_packet_size = DBC_MAX_PACKET_SIZE;

        // DW2/3
        ep.transfer_ring_dequeue_ptr = out_ring_->get_physical_dequeue_pointer_base();
        ep.dcs = out_ring_->get_cycle_bit();

        // DW4
        ep.average_trb_length = 0;
        ep.max_esit_payload_lo = 0;

        auto& ep_in = ctx->ep_in;

        ep_in.endpoint_state = 0;
        ep_in.mult = 0;
        ep_in.max_primary_streams = 0;
        ep_in.linear_stream_array = 0;
        ep_in.interval = 0;
        ep_in.max_esit_payload_hi = 0;

        ep_in.error_count = 3;
        ep_in.endpoint_type = DBC_EP_TYPE_BULK_IN;
        ep_in.host_initiate_disable = 0;
        ep_in.max_burst_size = max_burst;
        ep_in.max_packet_size = DBC_MAX_PACKET_SIZE;

        ep_in.transfer_ring_dequeue_ptr = in_ring_->get_physical_dequeue_pointer_base();
        ep_in.dcs = in_ring_->get_cycle_bit();

        ep_in.average_trb_length = 0;
        ep_in.max_esit_payload_lo = 0;

        // Allocate DMA bounce buffers

        write_buf_ = alloc_xhci_memory(DBC_BOUNCE_BUF_SIZE, 64, PAGE_SIZE);
        read_buf_ = alloc_xhci_memory(DBC_BOUNCE_BUF_SIZE, 64, PAGE_SIZE);

        if (!write_buf_ || !read_buf_) {
            Log::print_ln("xhci_dbc: failed to allocate bounce buffers\n");
            return false;
        }

        // 7. Allocate and program the DbC event ring
        // (programs DCERSTSZ, DCERDP, DCERSTBA internally)

        event_ring_ = new XhciDbcEventRing(regs_, DBC_EVENT_RING_TRB_COUNT);
        if (!event_ring_) {
            Log::print_ln("xhci_dbc: failed to allocate DbC event ring\n");
            return false;
        }

        // Write DCCP — physical address of DBC_CONTEXT

        regs_->dccp = xhci_get_physical_addr(dbc_ctx_) & DBC_DCCP_ADDR_MASK;

        // Write device descriptor info registers

        regs_->dcddi1.dbc_protocol = static_cast<u32>(dbc_protocol::VENDOR_DEFINED);
        regs_->dcddi1.vendor_id = DBC_VENDOR_ID;
        MMIO_POST_WRITE_PTR(&regs_->dcddi1);

        regs_->dcddi2.product_id = DBC_PRODUCT_ID;
        regs_->dcddi2.device_revision = DBC_DEVICE_REVISION;
        MMIO_POST_WRITE_PTR(&regs_->dcddi2);

        // Enable DbC
        // Per spec §7.6.4: set DCE after all other registers are programmed.

        regs_->dcctrl.dce = 1;
        regs_->dcctrl.lse = 1;  // generate Port Status Change Events on link status change
        MMIO_POST_WRITE_PTR(&regs_->dcctrl);

        return true;
    }

    bool XhciDbcPort::wait_for_connect(const u32 timeout_ms) const {
        for (u32 elapsed = 0; elapsed < timeout_ms; elapsed += 1) {
            xhci_trb_t evt{};
            while (event_ring_->poll_event(&evt)) { /* discard */
            }

            if (regs_->dcctrl.dcr) {
                Log::print_ln("xhci_dbc: debug host connected\n");

                DBC_DCPORTSC_REGISTER portsc{};
                portsc.raw = regs_->dcportsc.raw;
                portsc.csc = 1;
                portsc.prc = 1;
                portsc.plc = 1;
                portsc.cec = 1;
                regs_->dcportsc.raw = portsc.raw;
                MMIO_POST_WRITE_PTR(&regs_->dcportsc);

                Log::debug("Port num: %u", regs_->dcst.port_num);

                return true;
            }
            kernel::time::sleep_ms(10);
        }

        Log::print_ln("xhci_dbc: timed out waiting for debug host\n");
        return false;
    }

    bool XhciDbcPort::can_transfer() const {
        if (!regs_) {
            return false;
        }

        const auto ctrl = &regs_->dcctrl;
        const auto port = &regs_->dcportsc;
        const auto st = &regs_->dcst;

        // DbC globally enabled
        if (!ctrl->dce) {
            return false;
        }

        // Device not in configured state
        if (!ctrl->dcr) {
            return false;
        }

        // DbC configuration changed / disconnected / reset occurred
        if (ctrl->drc) {
            return false;
        }

        // Endpoint halted
        if (ctrl->hot || ctrl->hit) {
            return false;
        }

        // No debug host connected
        if (!port->ccs) {
            return false;
        }

        // Port not enabled
        if (!port->ped) {
            return false;
        }

        // Reset ongoing
        if (port->pr) {
            return false;
        }

        // Fatal LTSSM states
        switch (static_cast<dbc_port_link_state>(port->pls)) {
            case dbc_port_link_state::DISABLED:
            case dbc_port_link_state::INACTIVE:
            case dbc_port_link_state::HOT_RESET:
                return false;

            default:
                break;
        }

        // no debug port assigned
        if (st->port_num == 0) {
            return false;
        }

        return true;
    }

    bool XhciDbcPort::is_configured() const {
        return (regs_ != nullptr) && (regs_->dcctrl.dcr == 1u);
    }

    void XhciDbcPort::clear_run_change() const {
        if (!regs_) return;

        if (regs_->dcctrl.drc) {
            DBC_DCCTRL_REGISTER ctrl{};
            ctrl.raw = regs_->dcctrl.raw;
            ctrl.drc = 1;  // write-1-to-clear
            regs_->dcctrl.raw = ctrl.raw;
            MMIO_POST_WRITE_PTR(&regs_->dcctrl);
        }

        // Clear all RW1C status bits in DCPORTSC so that wait_for_reconnect()
        // sees a clean edge when the next connection event fires.
        DBC_DCPORTSC_REGISTER portsc{};
        portsc.raw = regs_->dcportsc.raw;
        portsc.csc = 1;
        portsc.prc = 1;
        portsc.plc = 1;
        portsc.cec = 1;
        regs_->dcportsc.raw = portsc.raw;
        MMIO_POST_WRITE_PTR(&regs_->dcportsc);
    }

    void XhciDbcPort::wait_for_reconnect() const {
        // Polls indefinitely — intended for use after a disconnect has been
        // detected and clear_run_change() has been called.
        //
        // We keep draining the event ring so that the hardware's event FIFO
        // never fills up and stalls the DbC state machine.
        //
        // Returns only when DCR=1 (DbC-Configured), identical post-condition
        // to wait_for_connect() returning true.

        while (true) {
            xhci_trb_t evt{};
            while (event_ring_->poll_event(&evt)) { /* drain, discard */
            }

            if (regs_->dcctrl.dcr) {
                DBC_DCPORTSC_REGISTER portsc{};
                portsc.raw = regs_->dcportsc.raw;
                portsc.csc = 1;
                portsc.prc = 1;
                portsc.plc = 1;
                portsc.cec = 1;
                regs_->dcportsc.raw = portsc.raw;
                MMIO_POST_WRITE_PTR(&regs_->dcportsc);

                Log::print_ln("xhci_dbc: debug host reconnected\n");
                return;
            }

            kernel::time::sleep_ms(10);
        }
    }

    namespace {

        /**
         * @brief Polls the DbC event ring for a Transfer Event TRB.
         *
         * Spins up to @p max_iters times calling @p ring->poll_event().
         * Filters for TRBs of type XHCI_TRB_TYPE_TRANSFER_EVENT.
         *
         * @param ring      DbC event ring to drain.
         * @param out       Filled with the Transfer Event TRB on success.
         * @param timeout_ns Timeout limit before giving up.
         *
         * @return true  if a Transfer Event was obtained within the limit.
         * @return false on timeout.
         */
        bool poll_transfer_event(XhciDbcEventRing* const ring, xhci_trb_t* const out, u64 const timeout_ns) {
            const u64 deadline = kernel::time::get_uptime_ns() + timeout_ns;

            while (kernel::time::get_uptime_ns() < deadline) {
                if (ring->poll_event(out)) {
                    if (out->trb_type == XHCI_TRB_TYPE_TRANSFER_EVENT) {
                        return true;
                    }
                }
                __asm__ volatile("pause" ::: "memory");
            }
            return false;
        }

        /**
         * @brief Returns the residual (bytes not transferred) from a Transfer Event.
         *
         * The Transfer Event status field carries the number of bytes *not* sent or
         * received (transfer_length residual), not the number of bytes transferred.
         */
        inline usize transfer_event_residual(const xhci_trb_t* evt) {
            const auto* te = reinterpret_cast<const xhci_transfer_completion_trb_t*>(evt);
            return static_cast<usize>(te->transfer_length);
        }

        /**
         * @brief Clears DCCTRL.DRC (DbC Run Change) if it is set.
         *
         * DRC is RW1C and disables the DCDB doorbell while asserted.
         * It must be cleared before ringing the doorbell after a reconnect.
         */
        inline void clear_drc_if_set(volatile DBC_REGS* const regs) {
            if (regs->dcctrl.drc) {
                DBC_DCCTRL_REGISTER ctrl{};
                ctrl.raw = regs->dcctrl.raw;

                ctrl.drc = 1;

                regs->dcctrl.raw = ctrl.raw;
                MMIO_POST_WRITE_PTR(&regs->dcctrl);
            }
        }

    }  // anonymous namespace

    // ============================================================================
    //  XhciDbcPort::write — synchronous IN transfer (target → host)
    // ============================================================================

    int XhciDbcPort::write(const u8* buf, usize len) const {
        if (!is_configured()) {
            return -1;
        }

        clear_drc_if_set(regs_);

        const u64 phys_write = xhci_get_physical_addr(write_buf_);
        auto* wbuf = static_cast<u8*>(write_buf_);

        usize offset = 0;

        while (offset < len) {
            const usize chunk = ((len - offset) > DBC_BOUNCE_BUF_SIZE) ? DBC_BOUNCE_BUF_SIZE : (len - offset);

            __builtin_memcpy(wbuf, buf + offset, chunk);

            // Build a Normal TRB for the IN ring (target → debug host).
            xhci_trb_t trb{};
            trb.parameter = phys_write;
            trb.status = static_cast<u32>(chunk) & 0x1FFFFu;  // trb_transfer_length
            trb.trb_type = XHCI_TRB_TYPE_NORMAL;
            trb.interrupt_on_completion = 1;
            // cycle_bit is set by XhciTransferRing::enqueue().

            out_ring_->enqueue(&trb);

            // Ring IN doorbell — ep_in_target = 1 (target stream ID 1 for no-streams bulk).
            regs_->dcdb.db_target = DBC_DB_TARGET_OUT;
            MMIO_POST_WRITE_PTR(&regs_->dcdb);

            // Poll event ring for a Transfer Event.
            xhci_trb_t event{};
            if (!poll_transfer_event(event_ring_, &event, DBC_TRANSFER_TIMEOUT_NS)) {
                Log::print_ln("xhci_dbc: write timed out waiting for transfer event\n");
                return -1;
            }

            const auto* te = reinterpret_cast<const xhci_transfer_completion_trb_t*>(&event);
            const u8 cc = static_cast<u8>(te->completion_code);

            if (cc != XHCI_TRB_COMPLETION_CODE_SUCCESS && cc != XHCI_TRB_COMPLETION_CODE_SHORT_PACKET) {
                Log::print_ln("xhci_dbc: write transfer error: ");
                Log::print_ln(trb_completion_code_to_string(cc));
                Log::print_ln("\n");
                return -1;
            }

            offset += chunk;
        }

        return 0;
    }

    // ============================================================================
    //  XhciDbcPort::read — synchronous OUT transfer (host → target)
    // ============================================================================

    int XhciDbcPort::read(u8* const buf, const usize max_len, usize* const out_len) const {
        if (!is_configured()) {
            return -1;
        }

        clear_drc_if_set(regs_);

        const usize recv_len = (max_len > DBC_BOUNCE_BUF_SIZE) ? DBC_BOUNCE_BUF_SIZE : max_len;
        const u64 phys_read = xhci_get_physical_addr(read_buf_);

        // Post a Normal TRB on the OUT ring — makes a buffer available to receive
        // data from the debug host (host → target, OUT from host's perspective).
        xhci_trb_t trb{};
        trb.parameter = phys_read;
        trb.status = static_cast<u32>(recv_len) & 0x1FFFFu;
        trb.trb_type = XHCI_TRB_TYPE_NORMAL;
        trb.interrupt_on_completion = 1;

        in_ring_->enqueue(&trb);

        // Ring OUT doorbell — ep_out_target = 0 (no streams).
        regs_->dcdb.db_target = DBC_DB_TARGET_IN;
        MMIO_POST_WRITE_PTR(&regs_->dcdb);

        // Poll for a Transfer Event.
        xhci_trb_t event{};
        if (!poll_transfer_event(event_ring_, &event, DBC_TRANSFER_TIMEOUT_NS)) {
            Log::print_ln("xhci_dbc: read timed out waiting for transfer event\n");
            return -1;
        }

        const auto* te = reinterpret_cast<const xhci_transfer_completion_trb_t*>(&event);
        const u8 cc = static_cast<u8>(te->completion_code);

        if (cc != XHCI_TRB_COMPLETION_CODE_SUCCESS && cc != XHCI_TRB_COMPLETION_CODE_SHORT_PACKET) {
            Log::print_ln("xhci_dbc: read transfer error: ");
            Log::print_ln(trb_completion_code_to_string(cc));
            Log::print_ln("\n");
            return -1;
        }

        const usize residual = transfer_event_residual(&event);
        const usize received = (recv_len >= residual) ? (recv_len - residual) : 0u;

        memcpy(buf, read_buf_, received);
        *out_len = received;

        return 0;
    }

}  // namespace usb
