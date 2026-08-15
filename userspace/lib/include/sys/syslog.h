// syslog.h
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
#ifndef _SYS_SYSLOG_H
#define _SYS_SYSLOG_H

#include <stdarg.h>

/* Options for openlog().  Bitwise-ORed into the `option` argument.  */
#define LOG_PID    0x01 /**< Log the process ID with each message.  */
#define LOG_CONS   0x02 /**< Log to the console if syslogd is unreachable.  */
#define LOG_ODELAY 0x04 /**< Delay opening the connection until the first syslog() call.  */
#define LOG_NDELAY 0x08 /**< Open the connection immediately (don't wait for the first message).  */
#define LOG_NOWAIT 0x10 /**< Don't wait for forked child processes used to log messages to the console.  */
#define LOG_PERROR 0x20 /**< Also write messages to stderr.  */

/* Facility codes */
#define LOG_KERN     (0 << 3)  /**< Kernel messages.  */
#define LOG_USER     (1 << 3)  /**< Random user-level messages.  */
#define LOG_MAIL     (2 << 3)  /**< Mail subsystem.  */
#define LOG_DAEMON   (3 << 3)  /**< System daemons without a separate facility value.  */
#define LOG_AUTH     (4 << 3)  /**< Security/authorization messages.  */
#define LOG_SYSLOG   (5 << 3)  /**< Messages generated internally by syslogd.  */
#define LOG_LPR      (6 << 3)  /**< Line printer subsystem.  */
#define LOG_NEWS     (7 << 3)  /**< Network news subsystem.  */
#define LOG_UUCP     (8 << 3)  /**< UUCP subsystem.  */
#define LOG_CRON     (9 << 3)  /**< Clock daemon (cron and at).  */
#define LOG_AUTHPRIV (10 << 3) /**< Security/authorization messages (private).  */
#define LOG_FTP      (11 << 3) /**< FTP daemon.  */

/* Reserved for local use.  */
#define LOG_LOCAL0 (16 << 3)
#define LOG_LOCAL1 (17 << 3)
#define LOG_LOCAL2 (18 << 3)
#define LOG_LOCAL3 (19 << 3)
#define LOG_LOCAL4 (20 << 3)
#define LOG_LOCAL5 (21 << 3)
#define LOG_LOCAL6 (22 << 3)
#define LOG_LOCAL7 (23 << 3)

#define LOG_NFACILITIES 24                    /**< Number of facilities currently defined.  */
#define LOG_FACMASK     0x03f8                /**< Mask to extract the facility part of a priority/facility pair.  */
#define LOG_FAC(p)      (((p) & LOG_FACMASK) >> 3) /**< Extract the facility number from a priority/facility pair.  */

/* Priorities, in order of decreasing severity.  Passed to syslog(),
   optionally bitwise-ORed with a facility.  */
#define LOG_EMERG   0 /**< System is unusable.  */
#define LOG_ALERT   1 /**< Action must be taken immediately.  */
#define LOG_CRIT    2 /**< Critical conditions.  */
#define LOG_ERR     3 /**< Error conditions.  */
#define LOG_WARNING 4 /**< Warning conditions.  */
#define LOG_NOTICE  5 /**< Normal but significant condition.  */
#define LOG_INFO    6 /**< Informational message.  */
#define LOG_DEBUG   7 /**< Debug-level message.  */

#define LOG_PRIMASK    0x07              /**< Mask to extract the priority part of a priority/facility pair.  */
#define LOG_PRI(p)     ((p) & LOG_PRIMASK) /**< Extract the priority number from a priority/facility pair.  */
#define LOG_MAKEPRI(fac, pri) ((fac) | (pri)) /**< Combine a facility and a priority into one value.  */

/* Mask macros for setlogmask(): build/test the bitmask of priorities to
   be logged, as opposed to the priority values themselves above.  */
#define LOG_MASK(pri)   (1 << (pri))          /**< Build the mask for one priority.  */
#define LOG_UPTO(pri)   ((1 << ((pri) + 1)) - 1) /**< Build the mask for all priorities through `pri`.  */

#ifdef __cplusplus
extern "C" {
#endif

/* Open connection to system logger. */
void openlog(const char* ident, int option, int facility);

/* Generate a log message using FMT string and option arguments. */
void syslog(int priority, const char* format, ...);

/* Generate a log message using FMT and using arguments pointed to by AP. */
void vsyslog(int priority, const char* format, va_list args);

/* Close descriptor used to write to system logger. */
void closelog(void);

/* Set the log mask level.  */
int setlogmask(int mask);

#ifdef __cplusplus
}
#endif


#endif //_SYS_SYSLOG_H
