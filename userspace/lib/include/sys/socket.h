// socket.h
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
#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H
#include "bits/alltypes.h"
#include <vespera/socket.h>

int socket(int domain, int type, int protocol);

int socketpair(int domain, int type, int protocol, int sv[2]);

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int listen(int sockfd, int backlog);

int accept4(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen, int flags);

int accept(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen);

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);

#endif //_SYS_SOCKET_H
