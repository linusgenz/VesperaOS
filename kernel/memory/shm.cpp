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
} // namespace

void shm_handle_acquire(void* res) {
    auto* shm = static_cast<ShmObject*>(res);
    shm->acquire_handle();
}

void shm_handle_destroy(void* res) {
    auto* shm = static_cast<ShmObject*>(res);
    shm->release_handle();
}

ShmObject::ShmObject(const char* name)
    : handle_count_(1) {
    if (name) {
        strncpy(name_, name, sizeof(name_) - 1);
        name_[sizeof(name_) - 1] = '\0';
    }
    lock_.init("shm_obj_lock");
}

ShmObject* ShmObject::create(const char* name, usize initial_size) {
    auto* shm = new ShmObject(name);
    if (!shm) return nullptr;

    if (initial_size > 0) {
        if (shm->resize(initial_size) != 0) {
            delete shm;
            return nullptr;
        }
    }

    return shm;
}

void ShmObject::acquire_handle() {
    SpinlockGuard g(lock_);
    handle_count_++;
}

phys_addr_t ShmObject::get_page(usize offset_in_bytes) {
    SpinlockGuard sg(lock_);

    usize page_idx = offset_in_bytes / PAGE_SIZE;
    if (page_idx >= page_count_) return make_phys(0);

    phys_addr_t phys = pages_[page_idx];

    if (phys_null(phys)) {
        phys = kernel::memory::request_page_phys();
        if (!phys_null(phys)) {
            memset(phys_to_virt(phys), 0, PAGE_SIZE);
            pages_[page_idx] = phys;
        }
    }

    return phys;
}

void ShmObject::sync_page(usize offset_in_bytes, phys_addr_t phys, bool is_dirty) {
    (void)offset_in_bytes;
    (void)phys;
    (void)is_dirty;
}

void ShmObject::add_mapping() {
    SpinlockGuard sg(lock_);
    mapping_count_++;
}

void ShmObject::remove_mapping() {
    release_mapping();
}

usize ShmObject::get_size() const {
    SpinlockGuard sg(lock_);
    return size_;
}

i64 ShmObject::resize(usize new_size) {
    SpinlockGuard g(lock_);

    usize new_page_count = (new_size + PAGE_SIZE - 1) / PAGE_SIZE;

    if (new_page_count == page_count_) {
        size_ = new_size;
        return 0;
    }

    if (new_page_count == 0) {
        if (pages_) {
            for (usize i = 0; i < page_count_; i++) {
                if (!phys_null(pages_[i])) {
                    kernel::memory::free_page_phys(pages_[i]);
                }
            }
            kernel::memory::free(pages_);
            pages_ = nullptr;
        }
        page_count_ = 0;
        size_ = 0;
        return 0;
    }

    auto* new_pages = static_cast<phys_addr_t*>(
        kernel::memory::malloc(new_page_count * sizeof(phys_addr_t))
    );
    if (!new_pages) return -ENOMEM;
    memset(new_pages, 0, new_page_count * sizeof(phys_addr_t));

    usize copy_count = (new_page_count < page_count_) ? new_page_count : page_count_;
    if (pages_ && copy_count > 0) {
        memcpy(new_pages, pages_, copy_count * sizeof(phys_addr_t));
    }

    if (new_page_count < page_count_) {
        for (usize i = new_page_count; i < page_count_; i++) {
            if (!phys_null(pages_[i])) {
                kernel::memory::free_page_phys(pages_[i]);
            }
        }
    }

    if (pages_) {
        kernel::memory::free(pages_);
    }

    pages_ = new_pages;
    page_count_ = new_page_count;
    size_ = new_size;

    return 0;
}

void ShmObject::mark_unlinked() {
    SpinlockGuard g(lock_);
    unlinked_ = true;
}

void ShmObject::check_and_destroy() const {
    bool dest = false;
    {
        SpinlockGuard g(lock_);
        if (handle_count_ == 0 && mapping_count_ == 0 && unlinked_) {
            dest = true;
        }
    }

    if (dest) {
        if (pages_) {
            for (usize i = 0; i < page_count_; i++) {
                if (!phys_null(pages_[i])) {
                    kernel::memory::free_page_phys(pages_[i]);
                }
            }
            kernel::memory::free(pages_);
        }
        delete this;
    }
}

void ShmObject::release_handle() {
    {
        SpinlockGuard g(lock_);
        handle_count_--;
    }
    check_and_destroy();
}

void ShmObject::release_mapping() {
    {
        SpinlockGuard g(lock_);
        mapping_count_--;
    }
    check_and_destroy();
}

i64 kernel::shm::shm_open(const char* name, int oflag, u32 mode, Realm* current_realm) {
    (void)mode;
    SpinlockGuard g(global_shm_lock);

    ShmObject* shm = nullptr;

    for (auto & global_shm_object : global_shm_objects) {
        if (global_shm_object && strcmp(global_shm_object->get_name(), name) == 0) {
            shm = global_shm_object;
            break;
        }
    }

    if (shm) {
        if ((oflag & O_CREAT) && (oflag & O_EXCL)) return -EEXIST;
        shm->acquire_handle();
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

        shm = ShmObject::create(name, 0);
        if (!shm) return -ENOMEM;

        global_shm_objects[free_slot] = shm;
    }

    auto res = current_realm->handle_table->add(
        HANDLE_TYPE_SHM, shm, CAP_READ | CAP_WRITE, true, shm_handle_destroy, shm_handle_acquire
    );

    if (!res.is_ok()) {
        shm->release_handle(); // Rollback
        return -ENOMEM;
    }

    return static_cast<i64>(res.unwrap());
}

i64 kernel::shm::shm_unlink(const char* name) {
    SpinlockGuard g(global_shm_lock);
    for (auto & global_shm_object : global_shm_objects) {
        if (global_shm_object && strcmp(global_shm_object->get_name(), name) == 0) {
            ShmObject* shm = global_shm_object;
            shm->mark_unlinked();
            global_shm_object = nullptr;
            return 0;
        }
    }
    return -ENOENT;
}

i64 kernel::shm::shm_truncate(unsigned long long handle_id, unsigned long length, Realm* current_realm) {
    if (!current_realm || !current_realm->handle_table) return -EINVAL;

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

    return shm->resize(length);
}