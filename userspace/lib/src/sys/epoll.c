// epoll.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 25.09.26.
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

#include <sys/epoll.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sysstd.h>

#include <vespera/handles.h>
#include <vespera/poll.h>

#include "poll.h"
#include "../internal/fd_table.h"

#define EPOLL_POLL_BATCH_MAX 1024

#define EPOLL_SENTINEL_HANDLE ((FILE_HANDLE)~(uintptr_t)1)

typedef struct epoll_watch {
    int fd;
    uint32_t events;
    epoll_data_t data;
    struct epoll_watch* next;
} epoll_watch_t;

typedef struct {
    int in_use;
    epoll_watch_t* watches;
    size_t count;
} epoll_instance_t;

#include "../internal/fd_table.h"  // FD_TABLE_MAX
static epoll_instance_t g_epoll_instances[FD_TABLE_MAX];

static short epoll_to_poll_events(uint32_t ev) {
    short p = 0;
    if (ev & EPOLLIN) p |= POLLIN;
    if (ev & EPOLLOUT) p |= POLLOUT;
    if (ev & EPOLLPRI) p |= POLLPRI;
    return p;
}

static uint32_t poll_to_epoll_events(short rev) {
    uint32_t e = 0;
    if (rev & POLLIN) e |= EPOLLIN;
    if (rev & POLLOUT) e |= EPOLLOUT;
    if (rev & POLLPRI) e |= EPOLLPRI;
    if (rev & POLLERR) e |= EPOLLERR;
    if (rev & POLLHUP) e |= EPOLLHUP;
    if (rev & POLLNVAL) e |= EPOLLERR;
    return e;
}

int epoll_create1(int flags) {
    if (flags != 0) {
        errno = EINVAL;
        return -1;
    }

    int fd = fd_table_insert(EPOLL_SENTINEL_HANDLE);
    if (fd < 0) {
        return -1;
    }

    if (fd < 0 || fd >= FD_TABLE_MAX) {
        fd_table_remove(fd);
        errno = EMFILE;
        return -1;
    }

    g_epoll_instances[fd].in_use = 1;
    g_epoll_instances[fd].watches = NULL;
    g_epoll_instances[fd].count = 0;

    return fd;
}

int epoll_create(int size) {
    if (size <= 0) {
        errno = EINVAL;
        return -1;
    }
    return epoll_create1(0);
}

static epoll_instance_t* epoll_get_instance(int epfd) {
    if (epfd < 0 || epfd >= FD_TABLE_MAX) return NULL;
    if (!g_epoll_instances[epfd].in_use) return NULL;
    if (fd_table_get(epfd) != EPOLL_SENTINEL_HANDLE) return NULL;
    return &g_epoll_instances[epfd];
}

static epoll_watch_t* epoll_find_watch(epoll_instance_t* inst, int fd) {
    for (epoll_watch_t* w = inst->watches; w; w = w->next) {
        if (w->fd == fd) return w;
    }
    return NULL;
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event) {
    epoll_instance_t* inst = epoll_get_instance(epfd);
    if (!inst) {
        errno = EBADH;
        return -1;
    }

    if (fd < 0 || !fd_table_valid(fd)) {
        errno = EBADH;
        return -1;
    }

    switch (op) {
        case EPOLL_CTL_ADD: {
            if (!event) {
                errno = EINVAL;
                return -1;
            }
            if (event->events & EPOLLET) {
                errno = EINVAL;
                return -1;
            }
            if (epoll_find_watch(inst, fd)) {
                errno = EEXIST;
                return -1;
            }

            epoll_watch_t* w = malloc(sizeof(epoll_watch_t));
            if (!w) {
                errno = ENOMEM;
                return -1;
            }
            w->fd = fd;
            w->events = event->events;
            w->data = event->data;
            w->next = inst->watches;
            inst->watches = w;
            inst->count++;
            return 0;
        }

        case EPOLL_CTL_MOD: {
            if (!event) {
                errno = EINVAL;
                return -1;
            }
            if (event->events & EPOLLET) {
                errno = EINVAL;
                return -1;
            }
            epoll_watch_t* w = epoll_find_watch(inst, fd);
            if (!w) {
                errno = ENOENT;
                return -1;
            }
            w->events = event->events;
            w->data = event->data;
            return 0;
        }

        case EPOLL_CTL_DEL: {
            epoll_watch_t** pp = &inst->watches;
            while (*pp) {
                if ((*pp)->fd == fd) {
                    epoll_watch_t* dead = *pp;
                    *pp = dead->next;
                    free(dead);
                    inst->count--;
                    return 0;
                }
                pp = &(*pp)->next;
            }
            errno = ENOENT;
            return -1;
        }

        default:
            errno = EINVAL;
            return -1;
    }
}

int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout) {
    epoll_instance_t* inst = epoll_get_instance(epfd);
    if (!inst) {
        errno = EBADH;
        return -1;
    }
    if (!events || maxevents <= 0) {
        errno = EINVAL;
        return -1;
    }

    if (inst->count == 0) {
        if (timeout == 0) return 0;
        (void)sys_poll(0, 0, (uint64_t)(int64_t)timeout, 0, 0, 0);
        return 0;
    }

    size_t n = inst->count;
    if (n > EPOLL_POLL_BATCH_MAX) {
        n = EPOLL_POLL_BATCH_MAX;
    }

    pollhdl_t hdls[EPOLL_POLL_BATCH_MAX];
    epoll_watch_t* watch_by_idx[EPOLL_POLL_BATCH_MAX];

    size_t idx = 0;
    for (epoll_watch_t* w = inst->watches; w && idx < n; w = w->next, idx++) {
        FILE_HANDLE h = fd_table_get(w->fd);
        if (h == INVALID_HANDLE) {
            hdls[idx].hdl = (int64_t)-1;
        } else {
            hdls[idx].hdl = (int64_t)h;
        }
        hdls[idx].events = epoll_to_poll_events(w->events);
        hdls[idx].revents = 0;
        watch_by_idx[idx] = w;
    }

    int64_t ret = sys_poll((uint64_t)(uintptr_t)hdls, (uint64_t)idx, (uint64_t)(int64_t)timeout, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }

    int out = 0;
    for (size_t i = 0; i < idx && out < maxevents; i++) {
        if (hdls[i].revents == 0) continue;

        uint32_t translated = poll_to_epoll_events((short)hdls[i].revents);
        uint32_t reported = translated & (watch_by_idx[i]->events | EPOLLERR | EPOLLHUP);
        if (reported == 0) continue;

        events[out].events = reported;
        events[out].data = watch_by_idx[i]->data;
        out++;
    }

    return out;
}

void epoll_close_hook(int epfd) {
    if (epfd < 0 || epfd >= FD_TABLE_MAX) return;
    if (!g_epoll_instances[epfd].in_use) return;

    epoll_watch_t* w = g_epoll_instances[epfd].watches;
    while (w) {
        epoll_watch_t* next = w->next;
        free(w);
        w = next;
    }
    g_epoll_instances[epfd].watches = NULL;
    g_epoll_instances[epfd].count = 0;
    g_epoll_instances[epfd].in_use = 0;
}
