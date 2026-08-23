// poll.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 23.08.26.
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

#include <poll.h>

#include <errno.h>
#include <stdlib.h>
#include <sysstd.h>

#include <vespera/handles.h>
#include <vespera/poll.h>
#include "internal/fd_table.h"

#define POLL_MAX_BATCH 1024

int poll(struct pollfd* fds, nfds_t nfds, int timeout) {
    if (fds == (struct pollfd*)0 && nfds != 0) {
        errno = EINVAL;
        return -1;
    }
    if (nfds == 0) {
        // Matches POSIX/Linux: polling zero fds just waits out the
        // timeout (useful as a portable sleep) and returns 0.
        if (timeout == 0) {
            return 0;
        }
        pollhdl_t dummy;
        (void)dummy;
        int64_t ret = sys_poll(0, 0, (uint64_t)(int64_t)timeout, 0, 0, 0);
        if (ret < 0) {
            errno = (int)(-ret);
            return -1;
        }
        return 0;
    }
    if (nfds > POLL_MAX_BATCH) {
        errno = EINVAL;
        return -1;
    }

    pollhdl_t hdls[POLL_MAX_BATCH];

    unsigned char resolved[POLL_MAX_BATCH];
    nfds_t nhdls = 0;

    for (nfds_t i = 0; i < nfds; i++) {
        fds[i].revents = 0;

        FILE_HANDLE handle = fd_table_get(fds[i].fd);
        if (handle == INVALID_HANDLE) {
            resolved[i] = 0;
            continue;
        }

        resolved[i] = 1;
        hdls[nhdls].hdl = (int64_t)handle;
        hdls[nhdls].events = (short)fds[i].events;
        hdls[nhdls].revents = 0;
        nhdls++;
    }

    int64_t ready = 0;

    if (nhdls > 0) {
        int64_t ret = sys_poll((uint64_t)(uintptr_t)hdls, (uint64_t)nhdls, (uint64_t)(int64_t)timeout, 0, 0, 0);
        if (ret < 0) {
            errno = (int)(-ret);
            return -1;
        }
        ready = ret;
    }

    nfds_t hdl_idx = 0;
    int nready = 0;
    for (nfds_t i = 0; i < nfds; i++) {
        if (!resolved[i]) {
            fds[i].revents = POLLNVAL;
            nready++;
            continue;
        }

        fds[i].revents = (short)hdls[hdl_idx].revents;
        if (fds[i].revents != 0) {
            nready++;
        }
        hdl_idx++;
    }

    (void)ready;
    return nready;
}