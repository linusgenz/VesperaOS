// xhci_dbc_manager.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 23.05.26.
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

/**
 * @brief Kernel-wide xHCI DbC output manager — TX ring, heartbeat, reconnect.
 *
 * Wraps an XhciDbcPort in a dedicated kernel Unit that drains a lock-protected
 * TX ring buffer, emits periodic heartbeats with timestamps, and transparently
 * handles disconnect/reconnect cycles.  Any kernel subsystem — including
 * interrupt handlers — can enqueue data via @ref XhciDbcManager::write without
 * blocking.
 *
 * @note Only one DbC port is supported at a time.
 * @note @ref register_port is not thread-safe; call it once from driver init.
 */

#ifndef VESPERAOS_DRIVERS_USB_XHCI_XHCI_DBC_MANAGER_H
#define VESPERAOS_DRIVERS_USB_XHCI_XHCI_DBC_MANAGER_H

#include <vespera/types.h>
class Spinlock;

namespace usb {

    class XhciDbcPort;

    /**
     * @brief Singleton manager for the xHCI Debug Capability port.
     */
    class XhciDbcManager {
       public:
        /**
         * @brief Initializes the manager with a connected DbC port and starts the
         *        background worker Unit.
         *
         * Spawns a kernel Unit on @p cpu_id that owns the TX drain loop, the
         * heartbeat timer, and the reconnect state machine.  The caller must not
         * touch @p port after this call — ownership is transferred to the manager.
         *
         * @param port    Fully initialized and connected @ref XhciDbcPort.
         * @param cpu_id  CPU to pin the worker Unit on.
         *
         * @note Call after @ref XhciDbcPort::wait_for_connect returns true.
         * @note Not thread-safe.  Call exactly once.
         */
        static void init(XhciDbcPort* port, u8 cpu_id);

        /**
         * @brief Enqueues raw bytes for transmission over the DbC bulk-IN endpoint.
         *
         * Copies @p len bytes into the internal TX ring buffer under a spinlock and
         * returns immediately.  If the ring is full the excess bytes are silently
         * dropped — this function never blocks.
         *
         * Safe to call from interrupt context.
         *
         * @param buf  Data to transmit.
         * @param len  Number of bytes in @p buf.
         */
        static void write(const u8* buf, usize len);

        /**
         * @brief Enqueues a null-terminated string prefixed with a kernel timestamp.
         *
         * Formats the line as `[SSSS.UUUUUU] <msg>\n` and enqueues it via
         * @ref write.  The timestamp is derived from @ref kernel::time::get_uptime_ns.
         *
         * Safe to call from interrupt context.
         *
         * @param msg  Null-terminated message string.
         */
        static void writeln(const char* msg);

        /**
         * @brief Returns true if a DbC port is registered and currently operational.
         */
        [[nodiscard]] static bool is_available();

       private:
        /** @brief TX ring buffer capacity in bytes. */
        static constexpr usize TX_RING_SIZE = 65536;

        /** @brief Heartbeat emission interval. */
        static constexpr u64 HEARTBEAT_INTERVAL_NS = 10'000'000'000ULL;

        /**
         * @brief Bytes pulled from the ring per drain iteration.
         *
         * Must not exceed @ref DBC_BOUNCE_BUF_SIZE (4 KiB) inside XhciDbcPort.
         */
        static constexpr usize DRAIN_CHUNK = 4096;

        /** @brief Milliseconds the worker sleeps between drain iterations when idle. */
        static constexpr u32 WORKER_IDLE_SLEEP_MS = 2;

        /** @brief Milliseconds the worker sleeps while waiting for can_transfer(). */
        static constexpr u32 WORKER_WAIT_SLEEP_MS = 10;

        static XhciDbcPort* port_;
        static bool available_;

        /** @brief Lock protecting all TX ring fields below. */
        static Spinlock tx_lock_;

        static u8 tx_ring_[TX_RING_SIZE];
        static usize tx_head_;  ///< Producer write index.
        static usize tx_tail_;  ///< Consumer read index.
        static usize tx_used_;  ///< Bytes currently enqueued.

        /** @brief Kernel Unit entry point for the TX/heartbeat worker. */
        static void worker_entry(void* arg);

        /**
         * @brief Drains up to @ref DRAIN_CHUNK bytes from the TX ring via the port.
         *
         * @param scratch  Caller-supplied scratch buffer of at least DRAIN_CHUNK bytes.
         *
         * @return Number of bytes actually transmitted (0 if ring was empty).
         */
        static usize drain_tx(u8* scratch);

        /**
         * @brief Handles a disconnect or error event.
         *
         * Clears DCCTRL.DRC (which unblocks the doorbell), logs the event, and then
         * blocks in @ref XhciDbcPort::wait_for_reconnect until the debug host
         * re-enumerates the device.  Returns only when DCR = 1.
         *
         * Must be called from the worker Unit whenever @ref XhciDbcPort::can_transfer
         * returns false after the port was previously operational.
         */
        static void recover();

        /**
         * @brief Formats and enqueues a heartbeat line.
         *
         * Emits `[HB SSSS.UUUUUU]\n` into the TX ring.
         */
        static void emit_heartbeat();

        /**
         * @brief Writes a timestamp prefix into @p buf.
         *
         * Format: `[SSSS.UUUUUU] ` (14 characters + NUL).
         *
         * @param buf       Destination buffer.
         * @param buf_size  Size of @p buf in bytes.
         *
         * @return Number of characters written, excluding the NUL terminator.
         */
        static usize format_timestamp(char* buf, usize buf_size);
    };

}  // namespace usb

#endif  // VESPERAOS_DRIVERS_USB_XHCI_XHCI_DBC_MANAGER_H