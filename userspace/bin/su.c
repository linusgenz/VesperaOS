// su.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 27.04.26.
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
#include <realm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysstd.h>

static void usage(void) {
    puts("Usage: su [options] [uid]");
    puts("       su --help");
    puts("");
    puts("Switch to another user identity.");
    puts("");
    puts("  uid          Numeric user ID to switch to (default: 0 = root)");
    puts("  -c <cmd>     Execute command as the target user and exit");
    puts("");
    puts("su sets uid, euid, gid, and egid to the target user's IDs.");
    puts("Only root may switch to an arbitrary user.");
    puts("A non-root user may only drop back to their saved uid.");
}

/*
 * Minimal password-file reader.
 * Reads /etc/passwd line by line looking for uid and returns
 * the associated primary gid.
 * Returns the gid on success, or (uint32_t)-1 if not found.
 *
 * Format: name:x:uid:gid:gecos:home:shell
 */
static uint32_t lookup_gid_for_uid(uint32_t target_uid) {
    /* Use a fixed-size stack buffer to avoid malloc. */
    static char line[256];

    FILE* f = fopen("/etc/passwd", "r");
    if (!f) return (uint32_t)-1;

    while (fgets(line, (int)sizeof(line), f)) {
        /* Skip comment lines */
        if (line[0] == '#') continue;

        /* field 0: name */
        char* p = line;
        char* next = strchr(p, ':');
        if (!next) continue;

        /* field 1: password placeholder */
        p = next + 1;
        next = strchr(p, ':');
        if (!next) continue;

        /* field 2: uid */
        p = next + 1;
        next = strchr(p, ':');
        if (!next) continue;
        *next = '\0';
        char* end;
        uint32_t uid = (uint32_t)strtoul(p, &end, 10);
        if (*end != '\0') continue;

        /* field 3: gid */
        p = next + 1;
        next = strchr(p, ':');
        if (!next) continue;
        *next = '\0';
        uint32_t gid = (uint32_t)strtoul(p, &end, 10);
        if (*end != '\0') continue;

        if (uid == target_uid) {
            fclose(f);
            return gid;
        }
    }

    fclose(f);
    return (uint32_t)-1;
}

int main(int argc, char** argv) {
    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        usage();
        return 0;
    }

    /* Parse arguments */
    uint32_t target_uid = 0; /* default: root */
    const char* command = NULL;
    int i = 1;

    while (i < argc) {
        if (strcmp(argv[i], "-c") == 0) {
            i++;
            if (i >= argc) {
                puts("su: -c requires an argument");
                return 1;
            }
            command = argv[i];
        } else if (argv[i][0] != '-') {
            /* Positional: target uid */
            char* end;
            target_uid = (uint32_t)strtoul(argv[i], &end, 10);
            if (*end != '\0') {
                printf("su: invalid user ID: '%s'\n", argv[i]);
                return 1;
            }
        } else {
            printf("su: unknown option: '%s'\n", argv[i]);
            return 1;
        }
        i++;
    }

    /* Resolve primary gid from /etc/passwd */
    uint32_t target_gid = lookup_gid_for_uid(target_uid);
    if (target_gid == (uint32_t)-1) {
        /*
         * /etc/passwd not found or uid not listed.
         * Fall back: use the same gid as the uid (common convention for
         * simple systems without a full user database).
         */
        target_gid = target_uid;
    }

    /* Set gid first (must be done before dropping uid privileges). */
    int64_t ret = sys_setgid((uint64_t)target_gid, 0, 0, 0, 0, 0);
    if (ret < 0) {
        if (ret == -EPERM) {
            printf("su: cannot set gid %u: Permission denied\n", target_gid);
        } else {
            printf("su: setgid failed: error %d\n", (int)ret);
        }
        return 1;
    }

    /* Now set uid. */
    ret = sys_setuid((uint64_t)target_uid, 0, 0, 0, 0, 0);
    if (ret < 0) {
        if (ret == -EPERM) {
            printf("su: cannot set uid %u: Permission denied\n", target_uid);
        } else {
            printf("su: setuid failed: error %d\n", (int)ret);
        }
        return 1;
    }

    if (command) {
        const char* shell_argv[] = {"/bin/sh", "-c", command, NULL};
        char* const envp[] = {NULL};

        RealmID r = spawn_realm("/bin/sh", (char* const*)shell_argv, envp, NULL);
        if ((int64_t)r < 0) {
            printf("su: spawn_realm failed: error %ld\n", (int64_t)r);
            return 1;
        }

        int status;
        wait_realm(r, &status);
        return status;
    }

    const char* shell_argv[] = {"/bin/sh", NULL};
    char* const envp[] = {NULL};

    RealmID r = spawn_realm("/bin/sh", (char* const*)shell_argv, envp, NULL);
    if ((int64_t)r < 0) {
        printf("su: cannot launch shell: error %ld\n", (int64_t)r);
        return 1;
    }

    int status;
    wait_realm(r, &status);
    return status;
}
