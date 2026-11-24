// socket_msg.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 24.09.26.
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
#include <sys/un.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sysstd.h>
#include <vespera/handles.h>

#include "../internal/fd_table.h"

#define MSG_MAX_FDS 32

#define MSG_KERNEL_CTRL_SIZE CMSG_SPACE(MSG_MAX_FDS * sizeof(int64_t))

typedef union {
    struct cmsghdr align;
    int64_t align8;
    unsigned char buf[MSG_KERNEL_CTRL_SIZE];
} kernel_ctrl_buf;

static int build_kernel_control(const struct msghdr* msg, kernel_ctrl_buf* out, socklen_t* out_len) {
    unsigned char* dst = out->buf;
    size_t dst_used = 0;

    const unsigned char* src = (const unsigned char*)msg->msg_control;
    size_t src_len = msg->msg_controllen;
    size_t off = 0;

    const size_t hdr_size = CMSG_ALIGN(sizeof(struct cmsghdr));

    while (off + sizeof(struct cmsghdr) <= src_len) {
        struct cmsghdr hdr;
        memcpy(&hdr, src + off, sizeof(hdr));

        // Header muss plausibel sein und komplett im Puffer liegen.
        if (hdr.cmsg_len < sizeof(struct cmsghdr) || hdr.cmsg_len > src_len - off) {
            errno = EINVAL;
            return -1;
        }

        size_t payload_len = hdr.cmsg_len - hdr_size;
        if (hdr.cmsg_len < hdr_size) {
            errno = EINVAL;
            return -1;
        }

        if (hdr.cmsg_level == SOL_SOCKET && hdr.cmsg_type == SCM_RIGHTS) {
            if (payload_len % sizeof(int) != 0) {
                errno = EINVAL;
                return -1;
            }
            size_t n = payload_len / sizeof(int);
            if (n > MSG_MAX_FDS) {
                errno = EINVAL;
                return -1;
            }

            size_t needed = CMSG_SPACE(n * sizeof(int64_t));
            if (dst_used + needed > sizeof(out->buf)) {
                errno = EINVAL;
                return -1;
            }

            struct cmsghdr out_hdr;
            out_hdr.cmsg_len = (socklen_t)CMSG_LEN(n * sizeof(int64_t));
            out_hdr.cmsg_level = SOL_SOCKET;
            out_hdr.cmsg_type = SCM_RIGHTS;
            memcpy(dst + dst_used, &out_hdr, sizeof(out_hdr));

            for (size_t i = 0; i < n; i++) {
                int fd;
                memcpy(&fd, src + off + hdr_size + i * sizeof(int), sizeof(fd));

                FILE_HANDLE h = fd_table_get(fd);
                if (h == INVALID_HANDLE) {
                    errno = EBADH;
                    return -1;
                }

                int64_t hid = (int64_t)h;
                memcpy(dst + dst_used + hdr_size + i * sizeof(int64_t), &hid, sizeof(hid));
            }

            dst_used += needed;
        } else {
            // Unbekannte cmsgs unveraendert durchreichen (Kernel entscheidet).
            size_t needed = CMSG_ALIGN(hdr.cmsg_len);
            if (dst_used + needed > sizeof(out->buf)) {
                errno = EINVAL;
                return -1;
            }
            memset(dst + dst_used, 0, needed);
            memcpy(dst + dst_used, src + off, hdr.cmsg_len);
            dst_used += needed;
        }

        off += CMSG_ALIGN(hdr.cmsg_len);
    }

    *out_len = (socklen_t)dst_used;
    return 0;
}

ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags) {
    FILE_HANDLE handle = fd_table_get(sockfd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return -1;
    }

    struct msghdr kernel_msg;
    kernel_ctrl_buf kernel_ctrl;
    const struct msghdr* to_send = msg;

    if (msg && msg->msg_control && msg->msg_controllen > 0) {
        socklen_t kernel_ctrl_len = 0;
        if (build_kernel_control(msg, &kernel_ctrl, &kernel_ctrl_len) < 0) {
            return -1;
        }

        kernel_msg = *msg;
        kernel_msg.msg_control = kernel_ctrl_len > 0 ? kernel_ctrl.buf : NULL;
        kernel_msg.msg_controllen = kernel_ctrl_len;
        to_send = &kernel_msg;
    }

    int64_t ret = sys_sendmsg((uint64_t)handle, (uint64_t)(uintptr_t)to_send, (uint64_t)flags, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (ssize_t)ret;
}

// ---------------------------------------------------------------------------
// recvmsg
// ---------------------------------------------------------------------------

ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
    FILE_HANDLE handle = fd_table_get(sockfd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return -1;
    }

    // Ohne Kontrollpuffer gibt es nichts zu uebersetzen.
    if (!msg || !msg->msg_control || msg->msg_controllen == 0) {
        int64_t ret = sys_recvmsg((uint64_t)handle, (uint64_t)(uintptr_t)msg, (uint64_t)flags, 0, 0, 0);
        if (ret < 0) {
            errno = (int)(-ret);
            return -1;
        }
        return (ssize_t)ret;
    }

    // Der Kernel schreibt in einen lokalen Puffer im 8-Byte-Format. So kann
    // der Aufrufer-Puffer (int-Format, kleiner) nie ueberlaufen.
    kernel_ctrl_buf kernel_ctrl;
    memset(&kernel_ctrl, 0, sizeof(kernel_ctrl));

    struct msghdr kernel_msg = *msg;
    kernel_msg.msg_control = kernel_ctrl.buf;
    kernel_msg.msg_controllen = sizeof(kernel_ctrl.buf);
    kernel_msg.msg_flags = 0;

    int64_t ret = sys_recvmsg((uint64_t)handle, (uint64_t)(uintptr_t)&kernel_msg, (uint64_t)flags, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }

    // Ab hier hat der Kernel die Handles bereits in unseren Prozess
    // installiert. Jeder Pfad muss sie entweder in fds verwandeln oder
    // schliessen -- sonst leaken sie.
    unsigned char* dst = (unsigned char*)msg->msg_control;
    size_t dst_cap = msg->msg_controllen;
    size_t dst_used = 0;

    const unsigned char* src = kernel_ctrl.buf;
    size_t src_len = kernel_msg.msg_controllen;
    size_t off = 0;

    int out_flags = kernel_msg.msg_flags;
    const size_t hdr_size = CMSG_ALIGN(sizeof(struct cmsghdr));

    while (off + sizeof(struct cmsghdr) <= src_len) {
        struct cmsghdr hdr;
        memcpy(&hdr, src + off, sizeof(hdr));

        if (hdr.cmsg_len < hdr_size || hdr.cmsg_len > src_len - off) {
            // Kaputte Kernel-Antwort. Nicht weiter parsen.
            out_flags |= MSG_CTRUNC;
            break;
        }

        size_t payload_len = hdr.cmsg_len - hdr_size;

        if (hdr.cmsg_level == SOL_SOCKET && hdr.cmsg_type == SCM_RIGHTS) {
            size_t n = payload_len / sizeof(int64_t);

            // Wie viele fds passen in den Puffer des Aufrufers?
            size_t avail = dst_cap - dst_used;
            size_t fit = avail > hdr_size ? (avail - hdr_size) / sizeof(int) : 0;
            if (fit > n) {
                fit = n;
            }

            size_t installed = 0;
            int fds[MSG_MAX_FDS];

            for (size_t i = 0; i < n; i++) {
                int64_t hid;
                memcpy(&hid, src + off + hdr_size + i * sizeof(int64_t), sizeof(hid));

                if (i >= fit) {
                    // Kein Platz mehr: Handle schliessen, MSG_CTRUNC melden.
                    sys_close((uint64_t)hid, 0, 0, 0, 0, 0);
                    out_flags |= MSG_CTRUNC;
                    continue;
                }

                int new_fd = fd_table_insert((FILE_HANDLE)hid);
                if (new_fd < 0) {
                    // fd-Tabelle voll: Handle schliessen, Rest wird ebenfalls
                    // geschlossen (fit wird auf i gekappt).
                    sys_close((uint64_t)hid, 0, 0, 0, 0, 0);
                    out_flags |= MSG_CTRUNC;
                    fit = i;
                    continue;
                }
                fds[installed++] = new_fd;
            }

            if (installed > 0) {
                struct cmsghdr out_hdr;
                out_hdr.cmsg_len = (socklen_t)CMSG_LEN(installed * sizeof(int));
                out_hdr.cmsg_level = SOL_SOCKET;
                out_hdr.cmsg_type = SCM_RIGHTS;

                // Nur so viel schreiben, wie wirklich im Puffer des Aufrufers
                // liegt. CMSG_SPACE rundet auf; das Padding darf am Ende des
                // Puffers fehlen (wie bei Linux beim letzten cmsg).
                size_t data_end = hdr_size + installed * sizeof(int);
                size_t padded = CMSG_SPACE(installed * sizeof(int));
                size_t room = dst_cap - dst_used;
                size_t zero_len = padded < room ? padded : room;

                memset(dst + dst_used, 0, zero_len);
                memcpy(dst + dst_used, &out_hdr, sizeof(out_hdr));
                memcpy(dst + dst_used + hdr_size, fds, installed * sizeof(int));

                dst_used += (data_end <= room && padded > room) ? room : padded;
            }
        } else {
            // Unbekannte cmsg unveraendert kopieren, falls Platz.
            size_t needed = CMSG_ALIGN(hdr.cmsg_len);
            if (dst_used + needed <= dst_cap) {
                memcpy(dst + dst_used, src + off, hdr.cmsg_len);
                memset(dst + dst_used + hdr.cmsg_len, 0, needed - hdr.cmsg_len);
                dst_used += needed;
            } else {
                out_flags |= MSG_CTRUNC;
            }
        }

        off += CMSG_ALIGN(hdr.cmsg_len);
    }

    msg->msg_controllen = (socklen_t)dst_used;
    msg->msg_flags = out_flags;
    return (ssize_t)ret;
}
