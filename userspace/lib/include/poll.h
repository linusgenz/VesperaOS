// poll.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 19.03.26.
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

#ifndef _POLL_H
#define _POLL_H

#include <vespera/poll.h>

#ifdef __cplusplus
extern "C" {
#endif


#define POLLNVAL 0x20

/// Priority/urgent-data flags. Not used currently, TODO implement
#define POLLPRI  0x10
#define POLLRDNORM POLLIN
#define POLLWRNORM POLLOUT
#define POLLRDBAND POLLPRI
#define POLLWRBAND POLLPRI

#ifndef _NFDS_T_DEFINED
#define _NFDS_T_DEFINED
typedef unsigned long nfds_t;
#endif

struct pollfd {
    int fd;         ///< fd-table slot to poll on, as returned by open()/opendir()/etc.
    short events;   ///< Requested events (bitwise OR of POLLIN, POLLOUT, ...)
    short revents;  ///< Events that actually occurred (filled in by poll())
};

/**
 * @brief Wait for events on a set of file descriptors.
 *
 * @param fds Array of `struct pollfd` describing the fds to watch and
 *            the events of interest in each entry's `events` field.
 *            On return, each entry's `revents` field is filled with the
 *            events that actually occurred (0 if none).
 * @param nfds Number of entries in `fds`.
 * @param timeout Timeout in milliseconds:
 *                  > 0 : wait at most this many milliseconds
 *                  0   : return immediately (non-blocking poll)
 *                  < 0 : block indefinitely until an event occurs
 *
 * @return On success, the number of `fds` entries with a nonzero
 *         `revents` (i.e. ready, error, or hangup). Returns 0 if the
 *         timeout expired before anything became ready. Returns -1 on
 *         error (errno set):
 *           EINVAL - `fds` is NULL and `nfds` != 0, or `nfds` exceeds
 *                    the maximum single sys_poll() call size
 *           EFAULT - `fds` pointer rejected by the kernel
 *
 * @note An invalid fd in one entry does not fail the whole call — as
 *       with Linux/POSIX, that entry's `revents` is set to POLLNVAL and
 *       `events` for that entry is ignored, same as glibc's poll().
 */
int poll(struct pollfd* fds, nfds_t nfds, int timeout);

#ifdef __cplusplus
}
#endif

#endif  // _POLL_H