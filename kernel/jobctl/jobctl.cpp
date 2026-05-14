// jobctl.cpp
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

#include <vespera/jobctl/jobctl.h>
#include <vespera/realm/realm_manager.h>

#include <realm/realm.h>
#include <tty/tty_device.h>

namespace kernel::jobctl {

    VoidResult assign_controlling_tty(const RealmId realm_id, TtyDevice* tty) {
        Realm* r = RealmManager::get(realm_id);
        if (!r)                          return Error::Srch;
        if (r->sid != r->id)             return Error::Perm;
        if (r->controlling_tty != nullptr) return Error::Perm;
        r->controlling_tty = tty;
        return VoidResult::ok();
    }

    bool is_controlling_tty(const RealmId realm_id, const TtyDevice* tty) {
        const Realm* r = RealmManager::get(realm_id);
        return r && r->controlling_tty == tty;
    }

    VoidResult set_foreground_pgid(const RealmId realm_id,
                                    TtyDevice* tty,
                                    const RealmId new_pgid) {
        if (!is_controlling_tty(realm_id, tty)) return Error::Perm;
        if (!tty->tty)                          return Error::NotTty;
        tty->tty->fg_pgid = new_pgid;
        return VoidResult::ok();
    }

    Result<RealmId> get_foreground_pgid(const TtyDevice* tty) {
        if (!tty || !tty->tty) return Error::NotTty;
        return Result<RealmId>::ok(tty->tty->fg_pgid);
    }

    Result<RealmId> create_session(RealmId realm_id) {
        Realm* realm = RealmManager::get(realm_id);
        if (!realm)
            return Error::Srch;

        // Already session leader? if so we fail
        if (realm->sid == realm->id)
            return Error::Perm;

        realm->controlling_tty = nullptr;

        realm->sid  = realm->id;
        realm->pgid = realm->id;

        return Result<RealmId>::ok(realm->id);
    }

    VoidResult set_pgid(
    RealmId caller_id,
    RealmId target_id,
    RealmId desired_pgid
) {
        Realm* caller = RealmManager::get(caller_id);
        if (!caller)
            return Error::Srch;

        Realm* target = RealmManager::get(target_id);
        if (!target)
            return Error::Srch;

        // session leader darf pgid nicht ändern
        if (target->sid == target->id)
            return Error::Perm;

        // nur innerhalb derselben session erlaubt
        if (target->sid != caller->sid)
            return Error::Perm;

        // pgid validation: muss existieren in session
        if (desired_pgid != target->id) {

            bool found = false;

            for (usize i = 0; i < RealmManager::MAX_REALMS; i++) {
                Realm* r = RealmManager::get(i + 1);
                if (!r || !r->active)
                    continue;

                if (r->sid == caller->sid && r->pgid == desired_pgid) {
                    found = true;
                    break;
                }
            }

            if (!found)
                return Error::Perm;
        }

        target->pgid = desired_pgid;
        return VoidResult::ok();
    }

} // namespace kernel::jobctl