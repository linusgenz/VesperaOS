// chown.c
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysstd.h>

static void usage(void) {
    puts("Usage: chown <owner>[:<group>] <file> [file...]");
    puts("       chown :<group>          <file> [file...]");
    puts("       chown --help");
    puts("");
    puts("Change file owner and/or group.");
    puts("");
    puts("  owner        Numeric user ID");
    puts("  :group       Numeric group ID (colon prefix)");
    puts("  owner:group  Both owner and group");
    puts("");
    puts("Only root may change the owner of a file.");
    puts("A non-root user may change the group to their own egid.");
}

/*
 * Parse "owner[:group]" or ":group".
 * Sets *out_uid and *out_gid; passes UINT32_MAX to leave a field unchanged.
 * Returns 0 on success, -1 on parse error.
 */
static int parse_owner_group(const char* spec, uint32_t* out_uid, uint32_t* out_gid) {
    *out_uid = (uint32_t)-1;
    *out_gid = (uint32_t)-1;

    /* Find optional colon separator */
    const char* colon = strchr(spec, ':');

    if (colon == spec) {
        /* ":group" form — no uid, only gid */
        const char* gid_str = colon + 1;
        if (*gid_str == '\0') return -1;
        char* end;
        unsigned long gid = strtoul(gid_str, &end, 10);
        if (*end != '\0') return -1;
        *out_gid = (uint32_t)gid;
        return 0;
    }

    if (colon) {
        /* "owner:group" form */
        /* Parse uid part */
        char uid_buf[32];
        size_t uid_len = (size_t)(colon - spec);
        if (uid_len == 0 || uid_len >= sizeof(uid_buf)) return -1;
        memcpy(uid_buf, spec, uid_len);
        uid_buf[uid_len] = '\0';

        char* end;
        unsigned long uid = strtoul(uid_buf, &end, 10);
        if (*end != '\0') return -1;
        *out_uid = (uint32_t)uid;

        /* Parse gid part (may be empty → leave unchanged) */
        const char* gid_str = colon + 1;
        if (*gid_str != '\0') {
            unsigned long gid = strtoul(gid_str, &end, 10);
            if (*end != '\0') return -1;
            *out_gid = (uint32_t)gid;
        }
        return 0;
    }

    /* No colon — only uid */
    char* end;
    unsigned long uid = strtoul(spec, &end, 10);
    if (*end != '\0') return -1;
    *out_uid = (uint32_t)uid;
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        usage();
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage();
        return 0;
    }

    uint32_t uid = (uint32_t)-1;
    uint32_t gid = (uint32_t)-1;

    if (parse_owner_group(argv[1], &uid, &gid) < 0) {
        printf("chown: invalid owner/group specification: '%s'\n", argv[1]);
        puts("       Use numeric IDs only (e.g. 1000, 1000:1000, :1000).");
        return 1;
    }

    int exit_code = 0;

    for (int i = 2; i < argc; i++) {
        const char* path = argv[i];

        int64_t ret = sys_chown((uint64_t)path, (uint64_t)uid, (uint64_t)gid, 0, 0, 0);
        if (ret < 0) {
            switch ((int)ret) {
                case -ENOENT:
                    printf("chown: cannot access '%s': No such file or directory\n", path);
                    break;
                case -EPERM:
                    printf("chown: changing ownership of '%s': Operation not permitted\n", path);
                    break;
                case -EROFS:
                    printf("chown: cannot change ownership of '%s': Read-only filesystem\n", path);
                    break;
                default:
                    printf("chown: cannot change ownership of '%s': error %d\n", path, (int)ret);
                    break;
            }
            exit_code = 1;
        }
    }

    return exit_code;
}
