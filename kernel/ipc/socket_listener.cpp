// socket_listener.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 22.09.26.
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


#include <vespera/ipc/socket_listener.h>
#include <vespera/scheduling.h>

#include "klib/result.h"
#include "klib/string.h"
#include "vespera/mm/memory.h"

Result<SocketListener*> SocketListener::bind(const char* path) {
    if (!path || path[0] == '\0') return Result<SocketListener*>::err(Error::Inval);

    auto* listener = new SocketListener();
    if (!listener) return Result<SocketListener*>::err(Error::NoMem);
    listener->path_ = strdup(path);

    return Result<SocketListener*>::ok(listener);
}

void SocketListener::unbind() {
    lock_.lock();
    unbound_ = true;
    listening_ = false;
    kernel::memory::free(const_cast<char*>(path_));

    // Drop any connections that were accepted-into-backlog but never claimed by accept()
    while (const BacklogEntry* entry = backlog_.pop()) {
        SocketEndpoint::destroy(entry->endpoint);
        delete entry;
    }
    backlog_count_ = 0;

    lock_.unlock();

    accept_wait_.wake_all();
    connect_wait_.wake_all();
}

void SocketListener::listen(const i32 backlog_capacity) {
    SpinlockGuard g(lock_);
    listening_ = true;
    backlog_capacity_ = backlog_capacity > 0 ? backlog_capacity : 1;
}

Result<SocketEndpoint*> SocketListener::connect(const usize capacity, const bool blocking) {
    while (true) {
        lock_.lock();

        if (unbound_) {
            lock_.unlock();
            return Result<SocketEndpoint*>::err(Error::ConnRefused);
        }

        if (!listening_) {
            lock_.unlock();
            return Result<SocketEndpoint*>::err(Error::ConnRefused);
        }

        if (backlog_count_ >= backlog_capacity_) {
            if (!blocking) {
                lock_.unlock();
                return Result<SocketEndpoint*>::err(Error::WouldBlock);
            }

            Unit* cur = kernel::scheduling::get_current_unit();
            connect_wait_.add_wait(cur);
            lock_.unlock();
            kernel::scheduling::yield();
            continue; // re-check state after waking
        }

        break; // room available, proceed to create the pair below
    }

    lock_.unlock();
    auto pair_result = SocketEndpoint::create_connected_pair(capacity);
    if (pair_result.is_err()) return Result<SocketEndpoint*>::err(pair_result.error());
    SocketEndpoint* client_side = pair_result.unwrap().a;
    SocketEndpoint* server_side = pair_result.unwrap().b;
    lock_.lock();

    if (unbound_) {
        lock_.unlock();
        SocketEndpoint::destroy(client_side);
        SocketEndpoint::destroy(server_side);
        return Result<SocketEndpoint*>::err(Error::ConnRefused);
    }

    auto* entry = new BacklogEntry{server_side, nullptr};
    if (!entry) {
        lock_.unlock();
        SocketEndpoint::destroy(client_side);
        SocketEndpoint::destroy(server_side);
        return Result<SocketEndpoint*>::err(Error::NoMem);
    }

    backlog_.push(entry);
    ++backlog_count_;

    lock_.unlock();

    accept_wait_.wake_one();

    return Result<SocketEndpoint*>::ok(client_side);
}

Result<SocketEndpoint*> SocketListener::accept(const bool blocking) {
    while (true) {
        lock_.lock();

        if (unbound_) {
            lock_.unlock();
            return Result<SocketEndpoint*>::err(Error::ConnAborted);
        }

        if (backlog_.empty()) {
            if (!blocking) {
                lock_.unlock();
                return Result<SocketEndpoint*>::err(Error::WouldBlock);
            }

            Unit* cur = kernel::scheduling::get_current_unit();
            accept_wait_.add_wait(cur);
            lock_.unlock();
            kernel::scheduling::yield();
            continue; // re-check state after waking
        }

        BacklogEntry* entry = backlog_.pop();
        --backlog_count_;
        lock_.unlock();

        SocketEndpoint* endpoint = entry->endpoint;
        delete entry;

        // A slot freed up in the backlog: wake a blocked connect(), if any.
        connect_wait_.wake_one();

        return Result<SocketEndpoint*>::ok(endpoint);
    }
}

void SocketListener::ref(void* res) {
    if (!res) return;
    auto* l = static_cast<SocketListener*>(res);
    __sync_add_and_fetch(&l->refcount_, 1);
}

void SocketListener::destroy(void* res) {
    if (!res) return;
    auto* l = static_cast<SocketListener*>(res);

    if (__sync_sub_and_fetch(&l->refcount_, 1) != 0)
        return;

    l->unbind();
    delete l;
}
