// xhci_dbc_manager.cpp
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

#include <drivers/usb/xhci/xhci_dbc_manager.h>
#include <klib/string.h>
#include <vespera/log.h>
#include <vespera/realm/realm_types.h>
#include <vespera/time.h>
#include <vespera/unit/unit_manager.h>
#include <vespera/unit_config.h>

#include "xhci_dbc.h"

namespace usb {

    XhciDbcPort* XhciDbcManager::port_ = nullptr;
    bool XhciDbcManager::available_ = false;

    Spinlock XhciDbcManager::tx_lock_;
    u8 XhciDbcManager::tx_ring_[TX_RING_SIZE];
    usize XhciDbcManager::tx_head_ = 0;
    usize XhciDbcManager::tx_tail_ = 0;
    usize XhciDbcManager::tx_used_ = 0;

    void XhciDbcManager::init(XhciDbcPort* port, const u8 cpu_id) {
        port_ = port;

        const UnitConfig cfg = {
            .name = "xhci-dbc",
            .cpu_id = cpu_id,
            .priority = 3,
            .stack_size = 0x4000,
            .initial_handles = nullptr,
            .initial_handle_count = 0,
            .is_idle = false,
            .is_user = false,
            .user_stack_size = 0,
        };

        const Unit* unit = UnitManager::create(kernel::realm::REALM_DRIVER, worker_entry, nullptr, &cfg);
        if (!unit) {
            Log::error("[ DbC ] Failed to spawn worker unit");
        }
    }

    void XhciDbcManager::write(const u8* buf, usize len) {
        if (!buf || len == 0) return;

        tx_lock_.lock();

        const usize free_bytes = TX_RING_SIZE - tx_used_;
        if (len > free_bytes) len = free_bytes;

        for (usize i = 0; i < len; ++i) {
            tx_ring_[tx_head_] = buf[i];
            tx_head_ = (tx_head_ + 1) % TX_RING_SIZE;
        }
        tx_used_ += len;

        tx_lock_.unlock();
    }

    void XhciDbcManager::writeln(const char* msg) {
        char prefix[20];
        usize prefix_len = format_timestamp(prefix, sizeof(prefix));

        write(reinterpret_cast<const u8*>(prefix), prefix_len);
        write(reinterpret_cast<const u8*>(msg), strlen(msg));

        static constexpr u8 nl = '\n';
        write(&nl, 1);
    }

    bool XhciDbcManager::is_available() {
        return available_;
    }

    // -------------------------------------------------------------------------
    //  Worker Unit entry point
    // -------------------------------------------------------------------------

    void XhciDbcManager::worker_entry(void* /*arg*/) {
        alignas(64) u8 scratch[DRAIN_CHUNK];

        while (!port_->can_transfer()) {
            kernel::time::sleep_ms(WORKER_WAIT_SLEEP_MS);
        }

        available_ = true;
        Log::debug("[ DbC ] Worker started, port ready");

        u64 last_heartbeat_ns = kernel::time::get_uptime_ns();

        while (true) {
            while (port_->can_transfer()) {
                drain_tx(scratch);

                const u64 now = kernel::time::get_uptime_ns();
                if (now - last_heartbeat_ns >= HEARTBEAT_INTERVAL_NS) {
                    emit_heartbeat();
                    last_heartbeat_ns = now;
                }

                kernel::time::sleep_ms(WORKER_IDLE_SLEEP_MS);
            }

            available_ = false;
            recover();
            available_ = true;

            last_heartbeat_ns = kernel::time::get_uptime_ns();
        }
    }

    usize XhciDbcManager::drain_tx(u8* scratch) {
        tx_lock_.lock();

        usize to_copy = tx_used_;
        if (to_copy > DRAIN_CHUNK) to_copy = DRAIN_CHUNK;

        for (usize i = 0; i < to_copy; ++i) {
            scratch[i] = tx_ring_[tx_tail_];
            tx_tail_ = (tx_tail_ + 1) % TX_RING_SIZE;
        }
        tx_used_ -= to_copy;

        tx_lock_.unlock();

        if (to_copy == 0) return 0;

        if (port_->write(scratch, to_copy) != 0) {
            Log::warning("[ DbC ] write() failed for %zu bytes - data lost", to_copy);
            return 0;
        }

        return to_copy;
    }

    void XhciDbcManager::recover() {
        Log::debug("[ DbC ] Disconnect or error detected - entering recovery");

        port_->clear_run_change();

        Log::debug("[ DbC ] Waiting for debug host to reconnect...");
        port_->wait_for_reconnect();

        Log::debug("[ DbC ] Recovery complete, resuming TX drain");
    }

    void XhciDbcManager::emit_heartbeat() {
        const u64 ns = kernel::time::get_uptime_ns();
        const u64 sec = ns / 1'000'000'000ULL;
        const u64 us = (ns % 1'000'000'000ULL) / 1000ULL;

        char buf[32];
        const int len = snprintf(
            buf,
            sizeof(buf),
            "[HB %04llu.%06llu]\n",
            static_cast<unsigned long long>(sec),
            static_cast<unsigned long long>(us)
        );
        if (len > 0) {
            write(reinterpret_cast<const u8*>(buf), static_cast<usize>(len));
        }
    }

    usize XhciDbcManager::format_timestamp(char* buf, const usize buf_size) {
        const u64 ns = kernel::time::get_uptime_ns();
        const u64 sec = ns / 1'000'000'000ULL;
        const u64 us = (ns % 1'000'000'000ULL) / 1000ULL;

        const int written = snprintf(
            buf, buf_size, "[%04llu.%06llu] ", static_cast<unsigned long long>(sec), static_cast<unsigned long long>(us)
        );
        return written > 0 ? static_cast<usize>(written) : 0;
    }

}  // namespace usb
