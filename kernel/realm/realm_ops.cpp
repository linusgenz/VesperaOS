// realm_ops.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 14.05.26.
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

#include <vespera/realm/realm_ops.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/realm/exit_code_table.h>
#include <vespera/signals.h>
#include <vespera/scheduling.h>
#include "realm.h"
#include "uapi/vespera/wait.h"

namespace kernel::realm {

    Result<RealmId> get_pgid(const RealmId id) {
        const Realm* r = RealmManager::get(id);
        if (!r) return Error::Srch;
        return Result<RealmId>::ok(r->pgid);
    }

    Result<RealmId> get_sid(const RealmId id) {
        const Realm* r = RealmManager::get(id);
        if (!r) return Error::Srch;
        return Result<RealmId>::ok(r->sid);
    }

    VoidResult set_pgid(const RealmId id, const RealmId pgid) {
        Realm* r = RealmManager::get(id);
        if (!r) return Error::Srch;
        r->pgid = pgid;
        return VoidResult::ok();
    }

    VoidResult send_signal(const RealmId id, const Signal sig) {
        Realm* r = RealmManager::get(id);
        if (!r) return Error::Srch;
        Unit* u = r->unit_list;
        if (!u) return Error::Srch;
        signal_send(u, sig);
        return VoidResult::ok();
    }


    // TODO, when blocking we have race conditions. realm could get deleted between RM::get and the targets spinlock, in the while (true) block
    Result<WaitResult> wait(const RealmId id, const u32 flags) {
        Unit* current = kernel::scheduling::get_current_unit();
        if (!current)
            return Error::Inval;

        {
            Realm* target = RealmManager::get(id);
            if (!target)
                return Error::Child;

            SpinlockGuard g(target->lock);

            if (target->exited) {
                int exit_code = 0;
                ExitCodeTable::consume(id, &exit_code);
                RealmManager::reap(id);
                return Result<WaitResult>::ok({true, exit_code});
            }

            if (flags & WAIT_FLAG_NOHANG)
                return Result<WaitResult>::ok({false, 0});

            target->waiter_present = true;
            target->wait_queue.add_wait(current);
        }

        while (true) {
            scheduling::yield();

            Realm* target = RealmManager::get(id);
            if (!target) {
                return Error::Child;
            }

            SpinlockGuard g(target->lock);
            if (!target->exited) continue;

            int exit_code = 0;
            ExitCodeTable::consume(id, &exit_code);
            RealmManager::reap(id);
            return Result<WaitResult>::ok({true, exit_code});
        }
    }

    usize get_realm_count() {
        return RealmManager::get_active_count();
    }
} // namespace kernel::realm