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

#include <uapi/vespera/handles.h>
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
        const auto cfg_ptr = reinterpret_cast<const spawn_config_t*>(arg3);

        if (!user_path) return -EINVAL;

        const Unit* caller = kernel::scheduling::get_current_unit();
        Realm* parent_realm = caller->parent;
        TtyDevice* tty_dev = parent_realm ? parent_realm->get_tty_device() : kernel::tty::tty_devices[0];

        const char* base = strrchr(user_path, '/');
        base = base ? base + 1 : user_path;

        const char* rname = (cfg_ptr && cfg_ptr->realm_name) ? cfg_ptr->realm_name : base;

        const RealmConfig cfg = {
            .name = rname,
            .capabilities = CAP_RW | CAP_DEVICE_ACCESS,
            .is_user = true,
        };

        Realm* new_realm = RealmManager::create(&cfg);
        if (!new_realm) return -ENOMEM;

        new_realm->parent_id = parent_realm ? parent_realm->id : 0;
        new_realm->pgid = new_realm->id;  // always a new process group

        if (cfg_ptr && cfg_ptr->bg_realm) {
            new_realm->sid = new_realm->id;
            new_realm->controlling_tty = nullptr;
            new_realm->handle_table.setup_standard_handles(nullptr);
        } else {
            new_realm->sid = parent_realm ? parent_realm->sid : new_realm->id;
            new_realm->controlling_tty = tty_dev;

            if (tty_dev) {
                new_realm->handle_table.setup_standard_handles(tty_dev);
            }
        }

        if (cfg_ptr) {
            if (cfg_ptr->uid != 0) {
                new_realm->cred.uid = cfg_ptr->uid;
                new_realm->cred.euid = cfg_ptr->uid;
                new_realm->cred.suid = cfg_ptr->uid;
            }

            if (cfg_ptr->gid != 0) {
                new_realm->cred.gid = cfg_ptr->gid;
                new_realm->cred.egid = cfg_ptr->gid;
                new_realm->cred.sgid = cfg_ptr->gid;
            }

            if (parent_realm) {
                auto transfer = [&](i64 src_hid, HandleId dst_fixed_id) -> bool {
                    if (src_hid == 0) return true;

                    const HandleEntry* he = parent_realm->handle_table.lookup(static_cast<HandleId>(src_hid));
                    if (!he || !he->transferable) return false;

                    if (he->acquire) he->acquire(he->resource);

                    if (HandleEntry* existing = new_realm->handle_table.lookup(dst_fixed_id))
                        new_realm->handle_table.release(dst_fixed_id);

                    return new_realm->handle_table.add_at(
                        dst_fixed_id,
                        he->type,
                        he->resource,
                        he->capabilities,
                        he->transferable,
                        he->destroy,
                        he->acquire
                    ).is_ok();
                };

                if (!transfer(cfg_ptr->stdin_handle, HANDLE_STDIN)) {
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
        } else {
            if (parent_realm) {
                memcpy(&new_realm->cred, &parent_realm->cred, sizeof(kernel::security::process_credentials));
            } else {
                memset(&new_realm->cred, 0, sizeof(kernel::security::process_credentials));
            }
        }

        char norm[256];
        if (!VFS::resolve_to_absolute(user_path, norm, sizeof(norm))) {
            RealmManager::destroy(new_realm->id);
            return -EINVAL;
        }

        char resolved[256];
        if (cfg_ptr && cfg_ptr->home && VFS::resolve_to_absolute(cfg_ptr->home, resolved, sizeof(resolved))) {
            strncpy(new_realm->cwd_path, resolved, sizeof(new_realm->cwd_path));
        } else if (parent_realm) {  // Inherit cwd from caller.
            strncpy(new_realm->cwd_path, parent_realm->cwd_path, sizeof(new_realm->cwd_path));
        }

        auto exec_res = VFS::open(norm);
        if (exec_res.is_err()) {
            RealmManager::destroy(new_realm->id);
            return exec_res.to_errno();
        }
        VfsNode* exec_node = exec_res.unwrap();

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
            .is_main_unit = true,
            .auto_schedule = false,
            .argv = argv,
            .envp = envp,
        };

        Unit* u = UnitManager::create(new_realm->id, reinterpret_cast<unit_entry_t>(elf.entry_point), nullptr, &ucfg);
        if (!u) {
            RealmManager::destroy(new_realm->id);
            return -EFAULT;
        }
        u->context.fs_base = elf.tls_base;

        const uptr heap_begin = (elf.load_end + 0xFFFULL) & ~0xFFFULL;
        u->heap_start = heap_begin;
        u->heap_end = heap_begin;

        kernel::scheduling::add_unit(u);

        return new_realm->id;
    }
}  // namespace syscalls::internal
