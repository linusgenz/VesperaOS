// fd_table.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.08.26.
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

#include "fd_table.h"

#include <errno.h>
#include <string.h>
#include <vespera/handles.h>

// TODO: not thread-safe. VesperaOS userspace threading (once units share
// an fd table across the same realm) will need a lock here — a simple
// spinlock/futex is enough since the critical sections are tiny. Tracked
// alongside the errno TLS TODO in unistd.c/sysstd.
typedef struct {
    HANDLE handle;
    int in_use;
} fd_entry_t;

static fd_entry_t g_fd_table[FD_TABLE_MAX];
static int g_fd_table_initialized = 0;

void fd_table_init(void) {
    if (g_fd_table_initialized) return;

    memset(g_fd_table, 0, sizeof(g_fd_table));

    g_fd_table[0].handle = HANDLE_STDIN;
    g_fd_table[0].in_use = 1;
    g_fd_table[1].handle = HANDLE_STDOUT;
    g_fd_table[1].in_use = 1;
    g_fd_table[2].handle = HANDLE_STDERR;
    g_fd_table[2].in_use = 1;

    g_fd_table_initialized = 1;
}

int fd_table_insert(HANDLE handle) {
    // Start scanning past the standard streams; not required for
    // correctness but avoids re-checking slots 0-2 on every open().
    for (int fd = 3; fd < FD_TABLE_MAX; fd++) {
        if (!g_fd_table[fd].in_use) {
            g_fd_table[fd].handle = handle;
            g_fd_table[fd].in_use = 1;
            return fd;
        }
    }
    errno = EMFILE;
    return -1;
}

int fd_table_insert_at(int fd, HANDLE handle) {
    if (fd < 0 || fd >= FD_TABLE_MAX) {
        errno = EBADH;
        return -1;
    }

    g_fd_table[fd].handle = handle;
    g_fd_table[fd].in_use = 1;
    return 0;
}

HANDLE fd_table_get(int fd) {
    if (fd < 0 || fd >= FD_TABLE_MAX || !g_fd_table[fd].in_use) {
        return INVALID_HANDLE;
    }
    return g_fd_table[fd].handle;
}

void fd_table_remove(int fd) {
    if (fd < 0 || fd >= FD_TABLE_MAX) return;
    g_fd_table[fd].in_use = 0;
    g_fd_table[fd].handle = 0;
}

int fd_table_valid(int fd) {
    if (fd < 0 || fd >= FD_TABLE_MAX) return 0;
    return g_fd_table[fd].in_use;
}
