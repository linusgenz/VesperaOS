// syslog.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.08.26.
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


#include "syslog.h"

#include <stdio.h>
#include <string.h>

#define DEFAULT_IDENT "<unknown>"

static const char* log_ident = DEFAULT_IDENT;
static int log_option = 0;
static int log_facility = LOG_USER;
static int log_mask = LOG_UPTO(LOG_DEBUG); /* everything enabled by default */
static int log_is_open = 0;

static const char* priority_name(int priority) {
    switch (LOG_PRI(priority)) {
        case LOG_EMERG: return "EMERG";
        case LOG_ALERT: return "ALERT";
        case LOG_CRIT: return "CRIT";
        case LOG_ERR: return "ERR";
        case LOG_WARNING: return "WARNING";
        case LOG_NOTICE: return "NOTICE";
        case LOG_INFO: return "INFO";
        case LOG_DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

void openlog(const char* ident, int option, int facility) {
    log_ident = ident ? ident : DEFAULT_IDENT;
    log_option = option;
    log_facility = facility;
    log_is_open = 1;

    /* LOG_ODELAY/LOG_NDELAY govern when a real backend connection would be
     * established; with no backend to connect to yet, both are no-ops here. */
}

void vsyslog(int priority, const char* format, va_list args) {
    if (!(log_mask & LOG_MASK(LOG_PRI(priority)))) return;

    if (!log_is_open) openlog(DEFAULT_IDENT, 0, LOG_USER);

    /* If the caller didn't OR in a facility, fall back to the one set by
     * openlog(), matching real syslog() semantics. */
    int facility = LOG_FAC(priority) ? (priority & LOG_FACMASK) : log_facility;
    (void)facility; /* not used for anything yet, since stdout has no notion of facility */

    printf("%s: <%s> ", log_ident, priority_name(priority));

    if (!format) {
        printf("\n");
        return;
    }

    vprintf(format, args);

    size_t len = strlen(format);
    if (len == 0 || format[len - 1] != '\n') printf("\n");
}

void syslog(int priority, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsyslog(priority, format, args);
    va_end(args);
}

void closelog(void) {
    log_ident = DEFAULT_IDENT;
    log_option = 0;
    log_facility = LOG_USER;
    log_is_open = 0;


}

int setlogmask(int mask) {
    int old = log_mask;
    if (mask != 0) log_mask = mask;
    return old;
}
