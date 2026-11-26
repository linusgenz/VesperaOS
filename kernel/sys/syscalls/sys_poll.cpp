// sys_poll.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 19.03.26.
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

#include <sys/handle_resolution.h>
#include <uapi/vespera/poll.h>
#include <vespera/scheduling.h>
#include <vespera/time.h>
#include <vespera/types.h>

#include "filesystem/vfs_handle.h"
#include "realm/realm.h"
#include "units/unit.h"
#include "vespera/sync/poll_table.h"

namespace syscalls::internal {
    namespace {
        struct PinnedSlot {
            HandleId hid{0};
            Pollable* pollable{nullptr};
            bool pinned{false};
        };

        class PinnedHandles {
            static constexpr usize INLINE_CAP = 16;

        public:
            PinnedHandles(HandleTable* table, const pollhdl* hdls, usize nhdls)
                : table_(table), count_(nhdls) {
                if (nhdls > INLINE_CAP) {
                    heap_slots_ = static_cast<PinnedSlot*>(
                        kernel::memory::malloc(nhdls * sizeof(PinnedSlot))
                    );
                    if (!heap_slots_) {
                        ok_ = false;
                        return;
                    }
                    slots_ = heap_slots_;
                }

                for (usize i = 0; i < nhdls; ++i) {
                    slots_[i] = {};
                    slots_[i].hid = hdls[i].hdl;
                    if (HandleEntry* he = table_->lookup_and_acquire(hdls[i].hdl)) {
                        slots_[i].pinned = true;
                        slots_[i].pollable = he->resource ? he->resource->as_pollable() : nullptr;
                    }
                }
            }

            ~PinnedHandles() {
                for (usize i = 0; i < count_; ++i) {
                    if (slots_[i].pinned) {
                        table_->release(slots_[i].hid);
                    }
                }
                if (heap_slots_) kernel::memory::free(heap_slots_);
            }

            bool ok() const { return ok_; }
            const PinnedSlot& operator[](usize i) const { return slots_[i]; }

        private:
            HandleTable* table_;
            usize count_;
            bool ok_{true};
            PinnedSlot inline_slots_[INLINE_CAP]{};
            PinnedSlot* heap_slots_{nullptr};
            PinnedSlot* slots_{inline_slots_};
        };

        int check_all(const PinnedHandles& pinned, pollhdl* hdls, usize nhdls, PollTable* pt) {
            int ready = 0;

            for (usize i = 0; i < nhdls; ++i) {
                hdls[i].revents = 0;

                if (!pinned[i].pinned || !pinned[i].pollable) {
                    hdls[i].revents = POLLNVAL;
                    ready++;
                    continue;
                }

                if (pt) pt->set_events(hdls[i].events);
                const int mask = pinned[i].pollable->poll(pt);
                const int always_reported = mask & (POLLERR | POLLHUP | POLLNVAL);
                const int requested = mask & hdls[i].events & ~(POLLERR | POLLHUP | POLLNVAL);

                hdls[i].revents = static_cast<i16>(always_reported | requested);
                if (hdls[i].revents) ready++;
            }

            return ready;
        }
    } // namespace

    i64 sys_poll(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        auto* hdls = reinterpret_cast<pollhdl*>(arg0);
        const usize nhdls = arg1;
        const i32 timeout_ms = static_cast<i32>(arg2);

        if (!hdls && nhdls > 0) return -EINVAL;

        Unit* cur = kernel::scheduling::get_current_unit();
        Realm* realm = cur ? cur->parent : nullptr;
        if (!cur || !realm || !realm->handle_table) return -ESRCH;

        const u64 deadline_ns = (timeout_ms >= 0)
                                    ? kernel::time::get_uptime_ns() + static_cast<u64>(timeout_ms) * 1'000'000ULL
                                    : U64_MAX;

        PinnedHandles pinned(realm->handle_table, hdls, nhdls);
        if (!pinned.ok()) return -ENOMEM;

        PollTable pt(cur);

        while (true) {
            int ready = check_all(pinned, hdls, nhdls, nullptr);
            if (ready > 0) return ready;
            if (timeout_ms == 0) return 0;
            if (deadline_ns != U64_MAX && kernel::time::get_uptime_ns() >= deadline_ns) return 0;

            pt.reset();
            ready = check_all(pinned, hdls, nhdls, &pt);
            if (ready > 0) return ready;
            if (deadline_ns != U64_MAX && kernel::time::get_uptime_ns() >= deadline_ns) return 0;

            pt.sleep(deadline_ns);
            pt.unregister_all();
        }
    }
} // namespace syscalls::internal
