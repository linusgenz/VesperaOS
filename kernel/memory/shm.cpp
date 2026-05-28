// shm.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 27.05.26.
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

#include <klib/string.h>
#include <realm/handle_table.h>
#include <realm/realm.h>
#include <vespera/log.h>
#include <uapi/vespera/fflags.h>
#include <vespera/mm/memory.h>
#include <vespera/mm/shm.h>
#include <vespera_errno.h>

namespace {
    ShmObject* global_shm_objects[128] = {nullptr};
    Spinlock global_shm_lock{"global_shm_lock"};
}  // namespace

void shm_handle_acquire(void* res) {
    auto* shm = static_cast<ShmObject*>(res);
    SpinlockGuard g(shm->lock);
    shm->handle_count++;
}

void shm_handle_destroy(void* res) {
    auto* shm = static_cast<ShmObject*>(res);
    shm->release_handle();
}

void ShmObject::release_handle() {
    bool dest = false;
    {
        SpinlockGuard g(lock);
        handle_count--;
        if (handle_count == 0 && mapping_count == 0 && unlinked) dest = true;
    }
    if (dest) {
        for (usize i = 0; i < page_count; i++) {
            if (!phys_null(pages[i])) kernel::memory::free_page_phys(pages[i]);
        }
        kernel::memory::free(pages);
        kernel::memory::free(this);
    }
}

void ShmObject::release_mapping() {
    bool dest = false;
    {
        SpinlockGuard g(lock);
        mapping_count--;
        if (handle_count == 0 && mapping_count == 0 && unlinked) dest = true;
    }
    if (dest) {
        for (usize i = 0; i < page_count; i++) {
            if (!phys_null(pages[i])) kernel::memory::free_page_phys(pages[i]);
        }
        kernel::memory::free(pages);
        kernel::memory::free(this);
    }
}

i64 kernel::shm::shm_open(const char* name, int oflag, u32 mode, Realm* current_realm) {
    SpinlockGuard g(global_shm_lock);

    ShmObject* shm = nullptr;
    int slot = -1;

    for (int i = 0; i < 128; i++) {
        if (global_shm_objects[i] && strcmp(global_shm_objects[i]->name, name) == 0) {
            shm = global_shm_objects[i];
            slot = i;
            break;
        }
    }

    if (shm) {
        if ((oflag & O_CREAT) && (oflag & O_EXCL)) return -EEXIST;
        SpinlockGuard sg(shm->lock);
        shm->handle_count++;
    } else {
        if (!(oflag & O_CREAT)) return -ENOENT;

        int free_slot = -1;
        for (int i = 0; i < 128; i++) {
            if (!global_shm_objects[i]) {
                free_slot = i;
                break;
            }
        }
        if (free_slot == -1) return -ENOMEM;

        shm = static_cast<ShmObject*>(kernel::memory::malloc(sizeof(ShmObject)));
        memset(shm, 0, sizeof(ShmObject));
        strncpy(shm->name, name, 63);
        shm->lock.init("shm_obj_lock");
        shm->handle_count = 1;

        global_shm_objects[free_slot] = shm;
    }

    auto res = current_realm->handle_table->add(
        HANDLE_TYPE_SHM, shm, CAP_READ | CAP_WRITE, true, shm_handle_destroy, shm_handle_acquire
    );

    if (!res.is_ok()) {
        shm->release_handle();  // Rollback
        return -ENOMEM;
    }

    return static_cast<i64>(res.unwrap());
}

i64 kernel::shm::shm_unlink(const char* name) {
    SpinlockGuard g(global_shm_lock);
    for (int i = 0; i < 128; i++) {
        if (global_shm_objects[i] && strcmp(global_shm_objects[i]->name, name) == 0) {
            ShmObject* shm = global_shm_objects[i];
            shm->lock.lock();
            shm->unlinked = true;
            shm->lock.unlock();
            global_shm_objects[i] = nullptr;  // Aus globaler Sicht gelöscht
            return 0;
        }
    }
    return -ENOENT;
}

i64 kernel::shm::shm_truncate(unsigned long long handle_id, unsigned long length, Realm* current_realm) {
    if (!current_realm || !current_realm->handle_table) return -EINVAL;

    // 1. Handle aus der Tabelle des aktuellen Realms holen
    // Hinweis: Passe 'lookup' oder 'get' an die echte API deiner HandleTable an
    auto entry = current_realm->handle_table->lookup(handle_id);
    if (!entry) {
        return -EBADH;
    }

    if (entry->type != HANDLE_TYPE_SHM) {
        return -EINVAL;
    }

    auto* shm = static_cast<ShmObject*>(entry->resource);
    if (!shm) {
        return -EINVAL;
    }

    // 2. Objekt sperren, um Race Conditions bei parallelen Trucates/Mmaps zu verhindern
    SpinlockGuard g(shm->lock);

    // Berechne die Anzahl der benötigten Pages (aufgerundet)
    usize new_page_count = (length + PAGE_SIZE - 1) / PAGE_SIZE;

    // Wenn sich die Page-Anzahl nicht ändert, müssen wir nur die logische Größe updaten
    if (new_page_count == shm->page_count) {
        shm->size = length;  // Falls dein ShmObject ein 'size'-Feld (in Bytes) besitzt
        return 0;
    }

    // Fall: Shared Memory wird komplett auf 0 gesetzt
    if (new_page_count == 0) {
        for (usize i = 0; i < shm->page_count; i++) {
            if (!phys_null(shm->pages[i])) {
                kernel::memory::free_page_phys(shm->pages[i]);
            }
        }
        if (shm->pages) {
            kernel::memory::free(shm->pages);
            shm->pages = nullptr;
        }
        shm->page_count = 0;
        shm->size = 0;
        return 0;
    }

    // 3. Neues Array für die physischen Seiteneinträge allokieren
    auto* new_pages = static_cast<phys_addr_t*>(kernel::memory::malloc(new_page_count * sizeof(phys_addr_t)));
    if (!new_pages) {
        return -ENOMEM;
    }

    for (usize i = 0; i < new_page_count; i++) {
        new_pages[i] = kernel::memory::request_page_phys();  // TODO lazy allocate later
    }

    // Bestehende physische Adressen in das neue Array übernehmen
    usize copy_count = (new_page_count < shm->page_count) ? new_page_count : shm->page_count;
    if (shm->pages && copy_count > 0) {
        memcpy(new_pages, shm->pages, copy_count * sizeof(phys_addr_t));
    }

    // 4. Wenn das Objekt verkleinert wird: Überschüssige physische Seiten freigeben
    if (new_page_count < shm->page_count) {
        for (usize i = new_page_count; i < shm->page_count; i++) {
            if (!phys_null(shm->pages[i])) {
                kernel::memory::free_page_phys(shm->pages[i]);
            }
        }
    }

    // Altes Array freigeben und durch das neue ersetzen
    if (shm->pages) {
        kernel::memory::free(shm->pages);
    }

    shm->pages = new_pages;
    shm->page_count = new_page_count;
    shm->size = length;  // Optionale Abspeicherung der exakten Byte-Größe

    return 0;
}