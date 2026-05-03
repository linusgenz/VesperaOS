// chmod.c
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
    puts("Usage: chmod <mode> <file> [file...]");
    puts("       chmod --help");
    puts("");
    puts("Change file permission bits.");
    puts("");
    puts("Mode can be octal (e.g. 755, 644) or symbolic (e.g. u+x, go-w, a=r).");
    puts("");
    puts("Symbolic mode syntax:  [ugoa][+-=][rwxst]");
    puts("  u  - user (owner)");
    puts("  g  - group");
    puts("  o  - other");
    puts("  a  - all (ugo)");
    puts("  +  - add permission");
    puts("  -  - remove permission");
    puts("  =  - set exact permission");
    puts("  r  - read    (4)");
    puts("  w  - write   (2)");
    puts("  x  - execute (1)");
    puts("  s  - setuid/setgid bit");
    puts("  t  - sticky bit");
}

/* Parse an octal mode string. Returns -1 on error. */
static int parse_octal(const char* s) {
    int val = 0;
    if (*s == '\0') return -1;
    while (*s) {
        if (*s < '0' || *s > '7') return -1;
        val = val * 8 + (*s - '0');
        s++;
    }
    if (val > 07777) return -1;
    return val;
}

/*
 * Apply a single symbolic clause (e.g. "u+x", "go-w", "a=r") to *mode.
 * The current stat mode is needed to handle the '=' operator correctly.
 * Returns 0 on success, -1 on parse error.
 */
static int apply_symbolic_clause(const char* clause, uint16_t* mode) {
    /* Parse who: u g o a (default = a) */
    uint16_t user_mask  = 0;
    uint16_t group_mask = 0;
    uint16_t other_mask = 0;

    const char* p = clause;
    int have_who = 0;

    while (*p == 'u' || *p == 'g' || *p == 'o' || *p == 'a') {
        have_who = 1;
        if (*p == 'u') user_mask  = 1;
        if (*p == 'g') group_mask = 1;
        if (*p == 'o') other_mask = 1;
        if (*p == 'a') { user_mask = group_mask = other_mask = 1; }
        p++;
    }

    if (!have_who) {
        /* No who specified — default to all */
        user_mask = group_mask = other_mask = 1;
    }

    /* Parse operator */
    char op = *p;
    if (op != '+' && op != '-' && op != '=') return -1;
    p++;

    /* Parse permissions */
    uint16_t bits = 0;
    while (*p) {
        switch (*p) {
            case 'r': bits |= 4; break;
            case 'w': bits |= 2; break;
            case 'x': bits |= 1; break;
            case 's': bits |= 8; break;  /* setuid/setgid — handled below */
            case 't': bits |= 16; break; /* sticky — handled below */
            default:  return -1;
        }
        p++;
    }

    /* Expand bits into actual mode mask */
    uint16_t add_mask = 0;

    if (user_mask) {
        if (bits & 4) add_mask |= 0400;
        if (bits & 2) add_mask |= 0200;
        if (bits & 1) add_mask |= 0100;
        if (bits & 8) add_mask |= 04000; /* setuid */
    }
    if (group_mask) {
        if (bits & 4) add_mask |= 0040;
        if (bits & 2) add_mask |= 0020;
        if (bits & 1) add_mask |= 0010;
        if (bits & 8) add_mask |= 02000; /* setgid */
    }
    if (other_mask) {
        if (bits & 4) add_mask |= 0004;
        if (bits & 2) add_mask |= 0002;
        if (bits & 1) add_mask |= 0001;
    }
    if (bits & 16) add_mask |= 01000; /* sticky */

    /* Build the scope mask (all bits that this who controls) */
    uint16_t scope = 0;
    if (user_mask)  scope |= 07700;
    if (group_mask) scope |= 02070;
    if (other_mask) scope |= 0007;
    /* always include sticky and setuid/setgid in scope when explicitly set */
    scope |= 07000;

    uint16_t perm = *mode & 0777u; /* current lower 9 bits */
    uint16_t special = *mode & 07000u;

    switch (op) {
        case '+':
            *mode |= add_mask;
            break;
        case '-':
            *mode &= (uint16_t)~add_mask;
            break;
        case '=':
            /* clear everything in scope, then set add_mask */
            *mode &= (uint16_t)~scope;
            *mode |= add_mask;
            break;
    }

    (void)perm;
    (void)special;
    return 0;
}

/*
 * Parse mode string and apply it to *mode.
 * Supports octal and comma-separated symbolic clauses.
 * Returns 0 on success, -1 on error.
 */
static int parse_mode(const char* modestr, uint16_t* mode) {
    /* Octal if first char is a digit */
    if (*modestr >= '0' && *modestr <= '9') {
        int val = parse_octal(modestr);
        if (val < 0) return -1;
        /* preserve file type bits, replace permission bits */
        *mode = (*mode & 0xF000u) | (uint16_t)(val & 07777);
        return 0;
    }

    /* Symbolic: split on ',' and apply each clause */
    char buf[64];
    strncpy(buf, modestr, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* tok = strtok(buf, ',');
    while (tok) {
        if (apply_symbolic_clause(tok, mode) < 0) return -1;
        tok = strtok(NULL, ',');
    }
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

    const char* modestr = argv[1];
    int exit_code = 0;

    for (int i = 2; i < argc; i++) {
        const char* path = argv[i];

        /*
         * We need the current mode to support symbolic operations.
         * Use sys_stat if available; for now we call sys_chmod with a
         * freshly-parsed octal so that symbolic modes that need the old
         * mode still work (they read *mode which starts at 0 for octal,
         * or we'd need stat). Since stat.h is available we use it.
         */
#ifdef HAVE_SYS_STAT
        struct stat st;
        if (stat(path, &st) < 0) {
            printf("chmod: cannot access '%s': No such file or directory\n", path);
            exit_code = 1;
            continue;
        }
        uint16_t mode = (uint16_t)(st.st_mode & 0xFFFFu);
#else
        /* Without stat we start from 0 for symbolic and use the octal value directly. */
        uint16_t mode = 0;
#endif

        if (parse_mode(modestr, &mode) < 0) {
            printf("chmod: invalid mode: '%s'\n", modestr);
            return 1;
        }

        int64_t ret = sys_chmod((uint64_t)path, (uint64_t)mode, 0, 0, 0, 0);
        if (ret < 0) {
            switch ((int)ret) {
                case -ENOENT:
                    printf("chmod: cannot access '%s': No such file or directory\n", path);
                    break;
                case -EPERM:
                    printf("chmod: changing permissions of '%s': Operation not permitted\n", path);
                    break;
                case -EROFS:
                    printf("chmod: cannot change permissions of '%s': Read-only filesystem\n", path);
                    break;
                default:
                    printf("chmod: cannot change permissions of '%s': error %d\n", path, (int)ret);
                    break;
            }
            exit_code = 1;
        }
    }

    return exit_code;
}
