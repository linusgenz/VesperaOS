// exit_code_table.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 22.03.26.
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
#ifndef VESPERAOS_EXIT_CODE_TABLE_H
#define VESPERAOS_EXIT_CODE_TABLE_H

#include <vespera/sync/spinlock.h>
#include <vespera/types.h>

struct ExitRecord {
    RealmId rid;
    int exit_code;
    bool valid;
};

class ExitCodeTable {
   public:
    static void store(RealmId rid, int code) {
        SpinlockGuard g(lock_);
        for (auto& r : records_) {
            if (!r.valid) {
                r = {rid, code, true};
                return;
            }
        }
    }

    static bool consume(RealmId rid, int* out) {
        SpinlockGuard g(lock_);
        for (auto& r : records_) {
            if (r.valid && r.rid == rid) {
                *out = r.exit_code;
                r.valid = false;
                return true;
            }
        }
        return false;
    }

   private:
    static constexpr usize MAX = 64;
    static ExitRecord records_[MAX];
    static Spinlock lock_;
};

inline ExitRecord ExitCodeTable::records_[ExitCodeTable::MAX] = {};
inline Spinlock ExitCodeTable::lock_;

#endif  // VESPERAOS_EXIT_CODE_TABLE_H
