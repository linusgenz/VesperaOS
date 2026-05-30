// spawn.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.05.26.
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

#include <exec/spawn.h>
#include <filesystem/vfs.h>
#include <filesystem/vfs_handle.h>
#include <realm/handle_table.h>
#include <realm/realm.h>
#include <tty/tty_device.h>
#include <uapi/vespera/mount.h>
#include <units/unit.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera/unit/unit_manager.h>
#include <vespera/unit_config.h>

#include "elf.h"

namespace kernel::exec {

    namespace {

        bool transfer_handle(Realm* src, Realm* dst, u64 src_hid, HandleId dst_fixed_id) {
            if (src_hid == 0) return true;

            const HandleEntry* he = src->handle_table->lookup(src_hid);
            if (!he || !he->transferable) return false;

            if (he->acquire) he->acquire(he->resource);

            if (dst->handle_table->lookup(dst_fixed_id)) dst->handle_table->release(dst_fixed_id);

            return dst->handle_table
                ->add_at(
                    dst_fixed_id, he->type, he->resource, he->capabilities, he->transferable, he->destroy, he->acquire
                )
                .is_ok();
        }

    }  // namespace

    Result<RealmId> spawn(const char* path, const char** argv, const char** envp, const spawn_config_t* cfg) {
        if (!path) return Error::Inval;

        Realm* parent = kernel::scheduling::get_current_realm();
        TtyDevice* tty = parent ? parent->get_tty_device() : tty::tty_devices[0];

        const char* base = strrchr(path, '/');
        base = base ? base + 1 : path;
        const char* rname = (cfg && cfg->realm_name) ? cfg->realm_name : base;

        const RealmConfig rcfg = {
            .name = rname,
            .capabilities = CAP_RW | CAP_DEVICE_ACCESS,
            .is_user = true,
        };

        Realm* r = RealmManager::create(&rcfg);
        if (!r) return Error::NoMem;

        // Session + process group
        r->parent_id = parent ? parent->id : 0;
        r->pgid = r->id;

        if (cfg && cfg->bg_realm) {
            // Background realm: detached session, no controlling TTY, stdio → /dev/null
            r->sid = r->id;
            r->controlling_tty = nullptr;
            if (r->handle_table->setup_stdio("/dev/null").is_err()) {
                RealmManager::destroy(r->id);
                return Error::NoMem;
            }
        } else {
            // Foreground realm: inherit session, stdio → /dev/tty
            r->sid = parent ? parent->sid : r->id;
            r->controlling_tty = tty;
            if (tty) {
                if (r->handle_table->setup_stdio("/dev/tty").is_err()) {
                    RealmManager::destroy(r->id);
                    return Error::NoMem;
                }
            }
        }

        // VBUS channel
        if (r->handle_table->setup_vbus().is_err()) {
            RealmManager::destroy(r->id);
            return Error::NoMem;
        }

        // Credentials
        if (cfg) {
            if (cfg->uid != 0) r->cred.uid = r->cred.euid = r->cred.suid = cfg->uid;
            if (cfg->gid != 0) r->cred.gid = r->cred.egid = r->cred.sgid = cfg->gid;

            if (parent) {
                if (!transfer_handle(parent, r, cfg->stdin_handle, HANDLE_STDIN) ||
                    !transfer_handle(parent, r, cfg->stdout_handle, HANDLE_STDOUT) ||
                    !transfer_handle(parent, r, cfg->stderr_handle, HANDLE_STDERR)) {
                    RealmManager::destroy(r->id);
                    return Error::BadH;
                }
            }
        } else if (parent) {
            memcpy(&r->cred, &parent->cred, sizeof(r->cred));
        }

        // CWD
        char norm[256];
        if (!VFS::resolve_to_absolute(path, norm, sizeof(norm))) {
            RealmManager::destroy(r->id);
            return Error::Inval;
        }

        if (cfg && cfg->home) {
            char resolved[256];
            if (VFS::resolve_to_absolute(cfg->home, resolved, sizeof(resolved)))
                strncpy(r->cwd_path, resolved, sizeof(r->cwd_path));
        } else if (parent) {
            strncpy(r->cwd_path, parent->cwd_path, sizeof(r->cwd_path));
        }

        // Exec check + ELF load
        auto exec_res = VFS::open(norm);
        if (exec_res.is_err()) {
            RealmManager::destroy(r->id);
            return exec_res.error();
        }
        VfsNode* node = exec_res.unwrap();
        const bool noexec = node->mount && (node->mount->flags & MS_NOEXEC);
        VFS::close(node);
        if (noexec) {
            RealmManager::destroy(r->id);
            return Error::Acces;
        }

        const ElfLoader::LoadResult elf = ElfLoader::load(norm, 0x500000, r);
        if (!elf.success) {
            RealmManager::destroy(r->id);
            return Error::NoExec;
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

        Unit* u = UnitManager::create(r->id, reinterpret_cast<unit_entry_t>(elf.entry_point), nullptr, &ucfg);
        if (!u) {
            RealmManager::destroy(r->id);
            return Error::Fault;
        }

        u->context.fs_base = elf.tls_base;
        const uptr heap_begin = (elf.load_end + 0xFFFULL) & ~0xFFFULL;
        u->heap_start = u->heap_end = heap_begin;

        kernel::scheduling::add_unit(u);
        return Result<RealmId>::ok(r->id);
    }

}  // namespace kernel::exec