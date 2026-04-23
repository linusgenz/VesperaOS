// log_client.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 21.04.26.
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

#ifndef VESPERAOS_USERSPACE_LIB_LOG_CLIENT_H
#define VESPERAOS_USERSPACE_LIB_LOG_CLIENT_H

#include <channel.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sysstd.h>
#include <time.h>
#include <vespera/handles.h>

static CHANNEL_HANDLE _lc_chan = 0;
static const char* _lc_tag = "?";

static inline CHANNEL_HANDLE _lc_read_chan_file(const char* service_name) {
    char path[128];
    snprintf(path, sizeof(path), "/run/services/%s.log_chan", service_name);

    for (int i = 0; i < 10; i++) {
        FILE* f = fopen(path, "r");
        if (f) {
            unsigned long long hid = 0;
            if (fscanf(f, "%llu", &hid) == 1 && hid > 0) {
                fclose(f);
                return (CHANNEL_HANDLE)hid;
            }
            fclose(f);
        }
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 50000000LL};
        sys_nanosleep((uint64_t)&ts, 0, 0, 0, 0, 0);
    }
    return 0;
}

static inline void log_client_init(const char* service_name) {
    _lc_tag = service_name;
    _lc_chan = _lc_read_chan_file(service_name);
}

static inline void log_write(const char* level, const char* msg) {
    struct timespec ts;
    sys_clock_gettime(0, (uint64_t)&ts, 0, 0, 0, 0);
    unsigned sd = (unsigned)(ts.tv_sec % 86400);

    char buf[4096];
    int n = snprintf(
        buf, sizeof(buf), "[%02u:%02u:%02u] [%-5s] [%s] %s\n", sd / 3600, (sd % 3600) / 60, sd % 60, level, _lc_tag, msg
    );
    if (n <= 0) return;
    if ((size_t)n >= sizeof(buf)) n = (int)sizeof(buf) - 1;

    if (_lc_chan) {
        if (channel_send(_lc_chan, buf, (size_t)n) >= 0) return;
    }

    sys_write(HANDLE_STDERR, (uint64_t)buf, (size_t)n, 0, 0, 0);
}

#define LOG(msg) log_write("LOG", (msg))
#define LOG_DEBUG(msg) log_write("DEBUG", (msg))
#define LOG_INFO(msg) log_write("INFO", (msg))
#define LOG_WARN(msg) log_write("WARN", (msg))
#define LOG_ERROR(msg) log_write("ERROR", (msg))

#define LOGF(fmt, ...)                                \
    do {                                              \
        char _b[1024];                                \
        snprintf(_b, sizeof(_b), (fmt), __VA_ARGS__); \
        log_write("LOG", _b);                         \
    } while (0)

#define LOG_INFOF(fmt, ...)                           \
    do {                                              \
        char _b[1024];                                \
        snprintf(_b, sizeof(_b), (fmt), __VA_ARGS__); \
        log_write("INFO", _b);                        \
    } while (0)

#define LOG_WARNF(fmt, ...)                           \
    do {                                              \
        char _b[1024];                                \
        snprintf(_b, sizeof(_b), (fmt), __VA_ARGS__); \
        log_write("WARN", _b);                        \
    } while (0)

#define LOG_ERRORF(fmt, ...)                          \
    do {                                              \
        char _b[1024];                                \
        snprintf(_b, sizeof(_b), (fmt), __VA_ARGS__); \
        log_write("ERROR", _b);                       \
    } while (0)

#endif  // VESPERAOS_USERSPACE_LIB_LOG_CLIENT_H
