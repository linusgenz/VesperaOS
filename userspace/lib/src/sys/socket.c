// socket.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 23.09.26.
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

#include <sys/socket.h>
#include <sys/ucred.h>
#include <sys/un.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sysstd.h>
#include <vespera/handles.h>

#include "../internal/fd_table.h"

int socket(int domain, int type, int protocol) {
    int64_t ret = sys_socket((uint64_t)domain, (uint64_t)type, (uint64_t)protocol, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }

    int fd = fd_table_insert((FILE_HANDLE)ret);
    if (fd < 0) {
        sys_close((uint64_t)ret, 0, 0, 0, 0, 0);
        errno = EMFILE;
        return -1;
    }
    return fd;
}

int socketpair(int domain, int type, int protocol, int sv[2]) {
    FILE_HANDLE handles[2];

    int64_t ret = sys_socketpair((uint64_t)domain, (uint64_t)type,
                                  (uint64_t)protocol, (uint64_t)(uintptr_t)handles,
                                  0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }

    int fd0 = fd_table_insert(handles[0]);
    if (fd0 < 0) {
        sys_close((uint64_t)handles[0], 0, 0, 0, 0, 0);
        sys_close((uint64_t)handles[1], 0, 0, 0, 0, 0);
        errno = EMFILE;
        return -1;
    }

    int fd1 = fd_table_insert(handles[1]);
    if (fd1 < 0) {
        sys_close((uint64_t)handles[1], 0, 0, 0, 0, 0);
        fd_table_remove(fd0);   // falls es sowas gibt – sonst close(fd0) über den fd-Pfad
        sys_close((uint64_t)handles[0], 0, 0, 0, 0, 0);
        errno = EMFILE;
        return -1;
    }

    sv[0] = fd0;
    sv[1] = fd1;
    return 0;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    FILE_HANDLE handle = fd_table_get(sockfd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return -1;
    }

    int64_t ret = sys_bind((uint64_t)handle, (uint64_t)(uintptr_t)addr, (uint64_t)addrlen, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int listen(int sockfd, int backlog) {
    FILE_HANDLE handle = fd_table_get(sockfd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return -1;
    }

    int64_t ret = sys_listen((uint64_t)handle, (uint64_t)backlog, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int accept4(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen, int flags) {
    FILE_HANDLE handle = fd_table_get(sockfd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return -1;
    }

    int64_t ret = sys_accept4((uint64_t)handle, (uint64_t)(uintptr_t)addr, (uint64_t)(uintptr_t)addrlen, flags, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }

    int new_fd = fd_table_insert((FILE_HANDLE)ret);
    if (new_fd < 0) {
        sys_close((uint64_t)ret, 0, 0, 0, 0, 0);
        errno = EMFILE;
        return -1;
    }
    return new_fd;
}

int accept(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen) {
    return accept4(sockfd, addr, addrlen, 0);
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    FILE_HANDLE handle = fd_table_get(sockfd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return -1;
    }

    int64_t ret = sys_connect((uint64_t)handle, (uint64_t)(uintptr_t)addr, (uint64_t)addrlen, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    if (level == SOL_LOCAL && optname == LOCAL_PEERCRED) {
        if (!optval || !optlen || *optlen < sizeof(struct xucred)) {
            errno = EINVAL;
            return -1;
        }
        struct xucred *c = (struct xucred *)optval;
        c->cr_version = XUCRED_VERSION;
        c->cr_uid = 0;
        c->cr_ngroups = 0;
        *optlen = sizeof(struct xucred);
        return 0;
    }
    errno = ENOPROTOOPT;
    return -1;
}