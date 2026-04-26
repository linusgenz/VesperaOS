// logd.c
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 01.10.25.
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

#include <channel.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sysstd.h>
#include <time.h>
#include <vespera/handles.h>

#include "ctype.h"
#include "stdbool.h"

#define MAX_LINE 4096
#define POLL_TIMEOUT_MS 500
#define LOGFILE_PATH "/var/log/system.log"
#define CHAN_FILE "/run/services/memoria.log_chan"
#define RETRY_ATTEMPTS 10
#define RETRY_DELAY_MS 50

static CHANNEL_HANDLE g_chan = 0;
static FILE* g_logfile = NULL;
static volatile int g_running = 1;

static void on_sigterm(int sig) {
    (void)sig;
    g_running = 0;
}

static CHANNEL_HANDLE read_chan_file(void) {
    for (int i = 0; i < RETRY_ATTEMPTS; i++) {
        FILE* f = fopen(CHAN_FILE, "r");
        if (f) {
            unsigned long long hid = 0;
            if (fscanf(f, "%llu", &hid) == 1 && hid > 0) {
                fclose(f);
                return (CHANNEL_HANDLE)hid;
            }
            fclose(f);
        }
        struct timespec ts = {.tv_sec = 0, .tv_nsec = RETRY_DELAY_MS * 1000000LL};
        sys_nanosleep((uint64_t)&ts, 0, 0, 0, 0, 0);
    }
    return 0;
}

static void try_open_logfile(void) {
    if (g_logfile) return;
    g_logfile = fopen(LOGFILE_PATH, "a");
}

/*
 * Lines from log_client already carry a [HH:MM:SS] prefix.
 * memoria forwards them unchanged. Only lines generated internally by memoria
 * (which start with a non-'[' character) get a timestamp prepended here.
 */
static void emit(const char* line, size_t len) {
    bool has_ts = (len > 0 && line[0] == '[' && isdigit(line[1]));

    char prefix[16] = {0};
    size_t plen = 0;

    if (!has_ts) {
        struct timespec ts;
        sys_clock_gettime(0, (uint64_t)&ts, 0, 0, 0, 0);
        unsigned sd = (unsigned)(ts.tv_sec % 86400);
        plen = (size_t)snprintf(prefix, sizeof(prefix), "[%02u:%02u:%02u] ", sd / 3600, (sd % 3600) / 60, sd % 60);
    }

    if (g_logfile) {
        if (!has_ts) fputs(prefix, g_logfile);
        fwrite(line, 1, len, g_logfile);
        if (!len || line[len - 1] != '\n') fputc('\n', g_logfile);
        fflush(g_logfile);
    } else {
        //sys_write(HANDLE_STDERR, (uint64_t)prefix, plen, 0, 0, 0);
        //sys_write(HANDLE_STDERR, (uint64_t)line, len, 0, 0, 0);
       // if (!len || line[len - 1] != '\n') sys_write(HANDLE_STDERR, (uint64_t)"\n", 1, 0, 0, 0);
    }
}

int main(void) {
    signal(SIGTERM, on_sigterm);

    try_open_logfile();

    g_chan = read_chan_file();

    struct pollhdl hdls[1];
    unsigned tick = 0;
    char line_buf[MAX_LINE];

    emit("[INFO ] memoria started", 23);

    while (g_running) {
        if (g_chan) {
            hdls[0].hdl = (int64_t)g_chan;
            hdls[0].events = POLLIN;
            poll(hdls, 1, POLL_TIMEOUT_MS);

            for (;;) {
                ssize_t n = channel_recv(g_chan, line_buf, MAX_LINE - 1);
                if (n <= 0) break;
                line_buf[n] = '\0';
                emit(line_buf, (size_t)n);
            }
        } else {
            struct timespec ts = {.tv_sec = 0, .tv_nsec = POLL_TIMEOUT_MS * 1000000LL};
            sys_nanosleep((uint64_t)&ts, 0, 0, 0, 0, 0);
        }

        if (++tick % 10 == 0) {
            try_open_logfile();
            tick = 0;
        }
    }

    emit("[INFO ] memoria shutting down", 28);
    if (g_logfile) fclose(g_logfile);
    return 0;
}
