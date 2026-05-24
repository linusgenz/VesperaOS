// xhci_dbc.h
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

#ifndef VESPERAOS_XHCI_DBC_H
#define VESPERAOS_XHCI_DBC_H

#include "xhci_dbc_regs.h"

class XhciTransferRing;

namespace usb {

#define MMIO_POST_WRITE(reg)             \
    do {                                 \
        volatile u32 __tmp = (reg)->raw; \
        (void)__tmp;                     \
    } while (0)

#define MMIO_POST_WRITE64(reg)           \
    do {                                 \
        volatile u64 __tmp = (reg)->raw; \
        (void)__tmp;                     \
    } while (0)

    class XhciDbcEventRing;

    /**
     * @brief xHCI Debug Capability (DbC) port driver.
     *
     * Manages the complete lifecycle of an xHCI USB Debug Device: context
     * allocation, endpoint and event-ring setup, and synchronous (poll-mode)
     * bulk data transfers for early-boot debugging.
     *
     * Only one instance should exist per DbC-capable xHC; create it on
     * detection of extended capability ID 0x0A and call @ref init immediately.
     *
     * @note All I/O is synchronous and poll-mode — no IRQs are used.  Do not
     *       call @ref write or @ref read from an interrupt context.
     * @note @ref init must be called before any other method.
     */
    class XhciDbcPort {
       public:
        XhciDbcPort() = default;
        ~XhciDbcPort() = default;

        XhciDbcPort(const XhciDbcPort&) = delete;
        XhciDbcPort& operator=(const XhciDbcPort&) = delete;

        /**
         * @brief Initialises the DbC hardware and enables DbC operation.
         *
         * Allocates the DbC context, USB string descriptors, IN/OUT transfer
         * rings, the DbC event ring, and programs all required DbC MMIO
         * registers.  Sets DCCTRL.DCE = 1 on success.
         *
         * @param regs  Pointer to the DbC MMIO register block discovered via
         *              xHCI extended capabilities (capability ID 0x0A).
         *
         * @return true on success.
         * @return false if any allocation fails.
         *
         * @note Must be called once, from a single thread, before any other method.
         */
        [[nodiscard]] bool init(volatile DBC_REGS* regs);

        /**
         * @brief Blocks until a Debug Host connects and the DbC reaches
         *        Configured state, or the timeout expires.
         *
         * Polls DCCTRL.dcr.  Returns as soon as the bit goes high.
         *
         * @param timeout_ms  Maximum wait time in milliseconds.
         *
         * @return true  if a Debug Host connected within the timeout.
         * @return false if the timeout expired before connection.
         */
        [[nodiscard]] bool wait_for_connect(u32 timeout_ms) const;

        bool can_transfer() const;

        /**
         * @brief Clears DCCTRL.DRC (DbC Run Change) and all RW1C DCPORTSC status bits.
         *
         * DRC is set whenever the DbC exits the Configured state (cable pull, host
         * reset, timeout, ...).  While DRC is asserted the DCDB doorbell is disabled,
         * so this method must be called before any subsequent write() or read() after
         * a disconnect/error, and before calling wait_for_reconnect().
         *
         * Safe to call even if DRC is not currently set (no-op in that case).
         *
         * @note Not ISR-safe. Call from the worker Unit only.
         */
        void clear_run_change() const;

        /**
         * @brief Blocks until the DbC re-enters the Configured state (DCR = 1).
         *
         * Intended to be called after a disconnect has been detected and
         * clear_run_change() has been called.  Unlike wait_for_connect() there is no
         * timeout — the method polls indefinitely, draining the event ring on each
         * iteration to prevent the hardware FIFO from stalling.
         *
         * On return the port is in the DbC-Configured state and DCR = 1 (identical
         * post-condition to wait_for_connect() returning true).
         *
         * @note Not ISR-safe. Call from the worker Unit only.
         */
        void wait_for_reconnect() const;

        /**
         * @brief Sends data to the Debug Host over the IN endpoint (target→host).
         *
         * Copies @p len bytes from @p buf into a DMA bounce buffer, enqueues a
         * Normal TRB on the IN transfer ring, rings the IN doorbell, then spins
         * on the DbC event ring until a Transfer Event arrives or the operation
         * times out.
         *
         * If @p len exceeds @ref DBC_BOUNCE_BUF_SIZE the transfer is split into
         * successive chunks of that size.
         *
         * @param buf  Source buffer (need not be DMA-accessible).
         * @param len  Number of bytes to send.
         *
         * @return 0 on success.
         * @return -1 if not configured, a transfer event error occurred, or the
         *         poll timeout was reached.
         */
        [[nodiscard]] int write(const u8* buf, usize len) const;

        /**
         * @brief Receives data from the Debug Host over the OUT endpoint (host→target).
         *
         * Posts a Normal TRB on the OUT transfer ring with a DMA bounce buffer,
         * rings the OUT doorbell, then polls until a Transfer Event arrives or
         * the operation times out.  Copies the received bytes into @p buf and
         * reports the actual received length through @p out_len.
         *
         * @param buf      Destination buffer (need not be DMA-accessible).
         * @param max_len  Capacity of @p buf; at most @ref DBC_BOUNCE_BUF_SIZE bytes
         *                 are received per call.
         * @param out_len  Set to the number of bytes actually received.
         *
         * @return 0 on success.
         * @return -1 if not configured, a transfer event error occurred, or the
         *         poll timeout was reached.
         */
        [[nodiscard]] int read(u8* buf, usize max_len, usize* out_len) const;

        /**
         * @brief Returns true when the DbC is in Configured state.
         *
         * Reflects DCCTRL.dcr — '1' means the Debug Host has enumerated the
         * Debug Device and bulk transfers are accepted.
         */
        [[nodiscard]] bool is_configured() const;

       private:
        volatile DBC_REGS* regs_ = nullptr;
        XhciDbcEventRing* event_ring_ = nullptr;  ///< Poll-mode DbC event ring.
        XhciTransferRing* in_ring_ = nullptr;     ///< IN TR: target→host (write path).
        XhciTransferRing* out_ring_ = nullptr;    ///< OUT TR: host→target (read path).
        void* dbc_ctx_ = nullptr;                 ///< DMA-mapped DBC_CONTEXT (192 B).
        void* write_buf_ = nullptr;               ///< IN-path DMA bounce buffer.
        void* read_buf_ = nullptr;                ///< OUT-path DMA bounce buffer.
    };

}  // namespace usb

#endif  // VESPERAOS_XHCI_DBC_H
