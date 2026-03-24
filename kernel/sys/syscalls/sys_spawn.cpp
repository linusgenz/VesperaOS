// sys_spawn.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 21.09.25.
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

#include <uapi/vespera/handels.h>
#include <uapi/vespera/mount.h>
#include <uapi/vespera/spawn.h>
#include <vespera/log.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera/tty/tty.h>
#include <vespera_errno.h>

#include "../../exec/elf.h"
#include "../../units/unit_manager.h"

namespace syscalls::internal {
    i64 sys_spawn(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64, u64) {
        const auto user_path = reinterpret_cast<const char*>(arg0);
        const auto argv = reinterpret_cast<const char**>(arg1);
        const auto envp = reinterpret_cast<const char**>(arg2);
        const auto cfg_ptr   = reinterpret_cast<const spawn_config_t*>(arg3);

        if (!user_path) return -EINVAL;

        const Unit* caller = kernel::scheduling::get_current_unit();
        Realm* parent_realm = caller ? RealmManager::get(caller->rid) : nullptr;
        TtyDevice* tty_dev = parent_realm ? parent_realm->get_tty_device() : kernel::tty::tty_devices[0];

        const char* base = strrchr(user_path, '/');
        base = base ? base + 1 : user_path;
        const RealmConfig cfg = {.name = base, .capabilities = CAP_RW | CAP_DEVICE_ACCESS, .is_user = true};

        Realm* new_realm = RealmManager::create(&cfg);
        if (!new_realm) return -ENOMEM;

        new_realm->setup_standard_handles(tty_dev);

        if (cfg_ptr && parent_realm) {
            auto transfer = [&](i64 src_hid, HandleId dst_fixed_id) -> bool {
                if (src_hid == 0) return true;

                HandleEntry* he = parent_realm->lookup_handle(static_cast<HandleId>(src_hid));
                if (!he || !he->transferable) return false;

                if (he->acquire) {
                    he->acquire(he->resource);
                }

                if (HandleEntry* existing = new_realm->lookup_handle(dst_fixed_id)) {
                    new_realm->release_handle(dst_fixed_id);
                }

                new_realm->add_handle_with_id(
                    dst_fixed_id,
                    he->type,
                    he->resource,
                    he->capabilities,
                    he->transferable,
                    he->destroy,
                    he->acquire
                );
                return true;
            };

            if (!transfer(cfg_ptr->stdin_handle,  HANDLE_STDIN))  {
                RealmManager::destroy(new_realm->id);
                return -EBADH;
            }
            if (!transfer(cfg_ptr->stdout_handle, HANDLE_STDOUT)) {
                RealmManager::destroy(new_realm->id);
                return -EBADH;
            }
            if (!transfer(cfg_ptr->stderr_handle, HANDLE_STDERR)) {
                RealmManager::destroy(new_realm->id);
                return -EBADH;
            }
        }

        char norm[256];
        if (!VFS::resolve_to_absolute(user_path, norm, sizeof(norm))) {
            return -EINVAL;
        }

        // set the cwd to the caller cwd
        strncpy(new_realm->cwd_path, parent_realm->cwd_path, sizeof(new_realm->cwd_path));

        VfsNode* exec_node = VFS::open(norm);
        if (!exec_node) return -ENOENT;

        if (exec_node->mount && (exec_node->mount->flags & MS_NOEXEC)) {
            VFS::close(exec_node);
            RealmManager::destroy(new_realm->id);
            return -EACCES;
        }
        VFS::close(exec_node);

        const ElfLoader::LoadResult elf = ElfLoader::load(norm, 0x500000, new_realm);

        if (!elf.success) {
            RealmManager::destroy(new_realm->id);
            return -ENOEXEC;
        }

        const UnitConfig ucfg = {
            .name = "main_unit",
            .cpu_id = 6,
            .priority = 5,
            .is_user = true,
            .auto_schedule = false,
            .argv = argv,
            .envp = envp,
        };

        Unit* u = UnitManager::create(new_realm->id, reinterpret_cast<unit_entry_t>(elf.entry_point), nullptr, &ucfg);
        if (!u) {
            RealmManager::destroy(new_realm->id);
            return -EFAULT;
        }

        const uptr heap_begin = (elf.load_end + 0xFFFULL) & ~0xFFFULL;
        u->heap_start = heap_begin;
        u->heap_end = heap_begin;

        if (tty_dev) {
            tty_dev->tty->fg_realm_id = new_realm->id;
        }

        kernel::scheduling::add_unit(u);

        return new_realm->id;
    }
}  // namespace syscalls::internal
