// socket_listener.h
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

#ifndef VESPERAOS_IPC_SOCKET_LISTENER_H
#define VESPERAOS_IPC_SOCKET_LISTENER_H

#include <klib/intrusive_queue.h>
#include <klib/result.h>
#include <vespera/ipc/socket.h>
#include <vespera/sync/spinlock.h>
#include <vespera/sync/wait_queue.h>
#include <vespera/types.h>
struct BacklogEntry {
    SocketEndpoint* endpoint;
    BacklogEntry* next;
};

// SocketListener owns:
//   - the bind() registration of a path (visible globally via a registry)
//   - the backlog of pending, already-connected SocketEndpoints waiting
//     for accept() to claim them
//
// It does NOT own any data path itself - that's SocketEndpoint's job.
// connect() looks a path up in the registry, creates a connected pair via
// SocketEndpoint::create_connected_pair(), keeps one side for the caller
// and pushes the other side into this listener's backlog for accept() to
// pick up.
class SocketListener {
public:
    static Result<SocketListener*> bind(const char* path);

    void listen(i32 backlog_capacity);

    Result<SocketEndpoint*> connect(usize capacity, bool blocking);

    Result<SocketEndpoint*> accept(bool blocking);

    void unbind();

    static void ref(void* res);
    static void destroy(void* res);

private:
    SocketListener() = default;

    const char* path_;
    bool listening_ = false;
    bool unbound_ = false;
    i32 backlog_capacity_ = 0;
    i32 backlog_count_ = 0;
    i32 refcount_ = 1;

    IntrusiveQueue<BacklogEntry> backlog_;
    WaitQueue accept_wait_;  // accept() blocks here until backlog non-empty
    WaitQueue connect_wait_; // connect() blocks here if backlog is full
    Spinlock lock_;
};


#endif //VESPERAOS_IPC_SOCKET_LISTENER_H
