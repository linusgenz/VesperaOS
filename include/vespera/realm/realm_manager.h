// realm_manager.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 19.09.25.
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

#ifndef VESPERAOS_REALM_MANAGER_H
#define VESPERAOS_REALM_MANAGER_H

#include <klib/result.h>
#include <vespera/realm/realm_config.h>
#include <vespera/sync/atomic.h>
#include <vespera/sync/spinlock.h>

#include "vespera/signals.h"

class Realm;

class RealmManager {
   public:
    static void initialize();
    static bool is_initialized();
    static Realm* create(const RealmConfig* cfg);
    static Realm* get(RealmId id);
    static bool destroy(RealmId id, int exit_code);
    static void finalize_locked(Realm& realm);
    static void reap(RealmId id);
    static void signal_pgid(RealmId pgid, Signal sig);
    static Result<usize> get_status(void* manager_ref, void* buffer, usize size, usize offset);
    static void list();
    static void abort(RealmId id);
    static constexpr usize MAX_REALMS = 64;

   private:
    static Realm* find_realm_locked(RealmId id);
    static Realm realms_[MAX_REALMS];
    static Spinlock global_lock_;
    static RealmId next_id_;
    static atomic_u8_t seq_;
    static bool initialized_;
};

#endif  // VESPERAOS_REALM_MANAGER_H