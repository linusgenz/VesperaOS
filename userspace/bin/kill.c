// kill.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 24.03.26.
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysstd.h>


#define SIG_CHLD 17
#define SIG_ALRM 14
#define SIG_PIPE 13
#define SIG_USR1 20
#define SIG_USR2 21
#define SIG_TERM 15
#define SIG_KILL  9
#define SIG_HUP   1
#define SIG_INT   2
#define SIG_STOP  19
#define SIG_CONT  18

static void usage(void) {
    puts("Usage: kill [-<signal>] <realm_id>...");
    puts("       kill --help");
    puts("");
    puts("Send a signal to a realm.");
    puts("");
    puts("Signals:");
    puts("  1  HUP   - Hangup");
    puts("  2  INT   - Interrupt");
    puts("  9  KILL  - Kill (cannot be caught)");
    puts(" 15  TERM  - Terminate (default)");
    puts(" 18  CONT  - Continue");
    puts(" 19  STOP  - Stop");
}

static int parse_signal(const char* s) {
    if (s[0] >= '0' && s[0] <= '9') {
        int sig = 0;
        for (int i = 0; s[i]; i++) {
            if (s[i] < '0' || s[i] > '9') return -1;
            sig = sig * 10 + (s[i] - '0');
        }
        return sig;
    }

    const char* name = s;
    if (strncmp(s, "SIG", 3) == 0) name = s + 3;

    if (strcmp(name, "HUP")  == 0) return SIG_HUP;
    if (strcmp(name, "INT")  == 0) return SIG_INT;
    if (strcmp(name, "KILL") == 0) return SIG_KILL;
    if (strcmp(name, "TERM") == 0) return SIG_TERM;
    if (strcmp(name, "STOP") == 0) return SIG_STOP;
    if (strcmp(name, "CONT") == 0) return SIG_CONT;

    return -1;
}

static uint64_t parse_rid(const char* s) {
    uint64_t rid = 0;
    for (int i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') return (uint64_t)-1;
        rid = rid * 10 + (uint64_t)(s[i] - '0');
    }
    return rid;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage();
        return 0;
    }

    int signal = SIG_TERM;
    int first_target = 1;

    // Parse optional signal argument
    if (argv[1][0] == '-' && argv[1][1] != '\0') {
        signal = parse_signal(argv[1] + 1);
        if (signal < 0) {
            printf("kill: invalid signal '%s'\n", argv[1] + 1);
            return 1;
        }
        first_target = 2;
    }

    if (first_target >= argc) {
        puts("kill: missing realm id");
        return 1;
    }

    int ret = 0;
    for (int i = first_target; i < argc; i++) {
        uint64_t rid = parse_rid(argv[i]);
        if (rid == (uint64_t)-1) {
            printf("kill: invalid realm id '%s'\n", argv[i]);
            ret = 1;
            continue;
        }

        int64_t result = sys_kill(rid, (uint64_t)signal, 0, 0, 0, 0);
        if (result < 0) {
            switch ((int)result) {
                case -ESRCH:
                    printf("kill: realm %s does not exist\n", argv[i]);
                    break;
                case -EPERM:
                    printf("kill: cannot signal realm %s: Permission denied\n", argv[i]);
                    break;
                case -EINVAL:
                    printf("kill: invalid signal %d\n", signal);
                    break;
                default:
                    printf("kill: cannot signal realm %s: %s\n", argv[i], strerror((int)result));
                    break;
            }
            ret = 1;
        }
    }

    return ret;
}