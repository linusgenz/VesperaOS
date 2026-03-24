// env.c
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

// This binary serves as both 'env' and 'printenv'.
// When invoked as 'printenv', it only prints values (or checks specific vars).
// When invoked as 'env', it can also set variables and run a command.

#include <errno.h>
#include <exec.h>
#include <realm.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysstd.h>

static void usage_env(void) {
    puts("Usage: env [OPTION]... [-] [NAME=VALUE]... [COMMAND [ARG]...]");
    puts("       env --help");
    puts("");
    puts("Set each NAME to VALUE in the environment and run COMMAND.");
    puts("If no COMMAND is given, print the current environment.");
    puts("");
    puts("Options:");
    puts("  -i    Start with an empty environment");
    puts("  -u NAME   Remove NAME from the environment");
}

static void usage_printenv(void) {
    puts("Usage: printenv [NAME]...");
    puts("       printenv --help");
    puts("");
    puts("Print environment variables.");
    puts("If NAME is given, print only that variable's value.");
    puts("Exit status is 0 if all NAMEs were found, 1 otherwise.");
}

static bool is_printenv(const char* argv0) {
    const char* base = argv0;
    for (const char* p = argv0; *p; p++) {
        if (*p == '/') base = p + 1;
    }
    return strcmp(base, "printenv") == 0;
}

static void print_all_env(void) {
    if (!environ) return;
    for (char** e = environ; *e; e++) {
        printf("%s\n", *e);
    }
}

static int run_printenv(int argc, char** argv) {
    if (argc < 2) {
        print_all_env();
        return 0;
    }

    int ret = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage_printenv();
            return 0;
        }

        const char* val = getenv(argv[i]);
        if (val) {
            printf("%s\n", val);
        } else {
            ret = 1;
        }
    }
    return ret;
}

static int run_env(int argc, char** argv) {
    if (argc >= 2 &&
        (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        usage_env();
        return 0;
    }

    bool ignore_env = false;
    int i = 1;

    // Parse options and NAME=VALUE pairs
    for (; i < argc; i++) {
        const char* arg = argv[i];

        if (strcmp(arg, "-") == 0 || strcmp(arg, "-i") == 0) {
            ignore_env = true;
            continue;
        }

        if (arg[0] == '-' && arg[1] == 'u') {
            // -u NAME  (may be -uNAME or -u NAME)
            const char* varname = NULL;
            if (arg[2] != '\0') {
                varname = arg + 2;
            } else if (i + 1 < argc) {
                varname = argv[++i];
            } else {
                puts("env: option -u requires an argument");
                return 1;
            }
            unsetenv(varname);
            continue;
        }

        const char* eq = strchr(arg, '=');
        if (eq) {
            char name_buf[256];
            size_t name_len = (size_t)(eq - arg);
            if (name_len >= sizeof(name_buf)) {
                puts("env: variable name too long");
                return 1;
            }
            strncpy(name_buf, arg, name_len);
            name_buf[name_len] = '\0';
            setenv(name_buf, eq + 1, 1);
            continue;
        }

        break;
    }

    if (ignore_env) {
        // TODO we leak memory here since we overwritte environ,
        static char* empty_env[] = { NULL };
        environ = empty_env;
    }

    if (i >= argc) {
        print_all_env();
        return 0;
    }

    const char* cmd = argv[i];
    const char* prog = find_executable(cmd);
    if (!prog) {
        printf("env: '%s': No such file or executable\n", cmd);
        return 127;
    }

    int64_t rid = spawn_realm(prog, argv + i, environ, NULL);
    if (rid < 0) {
        switch ((int)rid) {
            case -ENOENT:
                printf("env: '%s': No such file or directory\n", prog);
                break;
            case -ENOEXEC:
                printf("env: '%s': Not a valid executable\n", prog);
                break;
            case -EACCES:
                printf("env: '%s': Permission denied\n", prog);
                break;
            default:
                printf("env: cannot run '%s': %s\n", prog, strerror((int)rid));
                break;
        }
        return 126;
    }

    int status = 0;
    wait_realm(rid, &status);
    return status;
}

int main(int argc, char** argv) {
    if (is_printenv(argv[0])) {
        return run_printenv(argc, argv);
    } else {
        return run_env(argc, argv);
    }
}