// find.c
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

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysstd.h>
#include <vespera/stat.h>

#define MAX_PATH   512
#define MAX_NAME   256

typedef enum {
    TYPE_ANY,
    TYPE_FILE,
    TYPE_DIR,
} filter_type_t;

typedef struct {
    const char*  name_pattern;   // (NULL = any)
    filter_type_t type;
    int64_t      min_size;       // (-1 = unset)
    int64_t      max_size;       // (-1 = unset)
    int          max_depth;      // (-1 = unlimited)
    bool         print_null;     // separate output with \0
} find_opts_t;

static void usage(void) {
    puts("Usage: find [PATH] [EXPRESSION]");
    puts("       find --help");
    puts("");
    puts("Search for files in a directory hierarchy.");
    puts("");
    puts("Expressions:");
    puts("  -name <pattern>    Filter by filename (supports * wildcard)");
    puts("  -type f            Only regular files");
    puts("  -type d            Only directories");
    puts("  -size +N           Files larger than N bytes");
    puts("  -size -N           Files smaller than N bytes");
    puts("  -maxdepth N        Limit recursion depth");
    puts("  -print0            Separate results with NUL instead of newline");
}

static bool glob_match(const char* pattern, const char* str) {
    if (!pattern || pattern[0] == '\0') return true;

    const char* p = pattern;
    const char* s = str;
    const char* star_p = NULL;
    const char* star_s = NULL;

    while (*s) {
        if (*p == '*') {
            star_p = p++;
            star_s = s;
        } else if (*p == '?' || *p == *s) {
            p++;
            s++;
        } else if (star_p) {
            p = star_p + 1;
            s = ++star_s;
        } else {
            return false;
        }
    }

    while (*p == '*') p++;
    return *p == '\0';
}

static void build_path(char* out, size_t out_size,
                        const char* dir, const char* name) {
    if (strcmp(dir, "/") == 0)
        snprintf(out, out_size, "/%s", name);
    else
        snprintf(out, out_size, "%s/%s", dir, name);
}

static int do_find(const char* path, int depth, const find_opts_t* opts) {
    if (opts->max_depth >= 0 && depth > opts->max_depth) return 0;

    DIR_HANDLE hdl = opendir(path);
    if (hdl < 0) {
        if (hdl == -ENOENT)
            printf("find: '%s': No such file or directory\n", path);
        else
            printf("find: '%s': %s\n", path, strerror((int)hdl));
        return 1;
    }

    dirent_t ent;
    char full[MAX_PATH];
    int ret = 0;

    while (readdir(hdl, &ent) > 0) {
        // Always skip . and ..
        if (strcmp(ent.name, ".") == 0 || strcmp(ent.name, "..") == 0) continue;

        build_path(full, sizeof(full), path, ent.name);

        if (opts->type == TYPE_FILE && ent.type == DT_DIR)  goto recurse;
        if (opts->type == TYPE_DIR  && ent.type != DT_DIR)  goto skip;

        if (opts->name_pattern && !glob_match(opts->name_pattern, ent.name)) {
            goto recurse;
        }

        if ((opts->min_size >= 0 || opts->max_size >= 0) && ent.type != DT_DIR) {
            vespera_stat_t st;
            if (sys_stat((uint64_t)full, (uint64_t)&st, 0, 0, 0, 0) == 0) {
                if (opts->min_size >= 0 && (int64_t)st.size <= opts->min_size) {
                    goto recurse;
                }
                if (opts->max_size >= 0 && (int64_t)st.size >= opts->max_size) {
                    goto recurse;
                }
            }
        }

        if (opts->print_null) {
            printf("%s%c", full, '\0');
        } else {
            printf("%s\n", full);
        }

    recurse:
        if (ent.type == DT_DIR) {
            ret |= do_find(full, depth + 1, opts);
        }
        continue;

    skip:
        (void)0;
    }

    closedir(hdl);
    return ret;
}

int main(int argc, char** argv) {
    if (argc >= 2 &&
        (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        usage();
        return 0;
    }

    find_opts_t opts = {
        .name_pattern = NULL,
        .type         = TYPE_ANY,
        .min_size     = -1,
        .max_size     = -1,
        .max_depth    = -1,
        .print_null   = false,
    };

    const char* root = NULL;
    int i = 1;

    if (argc > 1 && argv[1][0] != '-') {
        root = argv[1];
        i = 2;
    }

    for (; i < argc; i++) {
        if (strcmp(argv[i], "-name") == 0) {
            if (++i >= argc) { puts("find: -name requires an argument"); return 1; }
            opts.name_pattern = argv[i];

        } else if (strcmp(argv[i], "-type") == 0) {
            if (++i >= argc) { puts("find: -type requires an argument"); return 1; }
            if (strcmp(argv[i], "f") == 0)      opts.type = TYPE_FILE;
            else if (strcmp(argv[i], "d") == 0) opts.type = TYPE_DIR;
            else { printf("find: unknown type '%s'\n", argv[i]); return 1; }

        } else if (strcmp(argv[i], "-size") == 0) {
            if (++i >= argc) { puts("find: -size requires an argument"); return 1; }
            const char* sarg = argv[i];
            if (sarg[0] == '+') {
                opts.min_size = (int64_t)atoi(sarg + 1);
            } else if (sarg[0] == '-') {
                opts.max_size = (int64_t)atoi(sarg + 1);
            } else {
                int64_t exact = (int64_t)atoi(sarg);
                opts.min_size = exact - 1;
                opts.max_size = exact + 1;
            }

        } else if (strcmp(argv[i], "-maxdepth") == 0) {
            if (++i >= argc) { puts("find: -maxdepth requires an argument"); return 1; }
            opts.max_depth = atoi(argv[i]);

        } else if (strcmp(argv[i], "-print0") == 0) {
            opts.print_null = true;

        } else {
            printf("find: unknown expression '%s'\n", argv[i]);
            return 1;
        }
    }

    if (!root) {
        char cwd[MAX_PATH];
        if (getcwd(cwd, sizeof(cwd)) < 0) {
            strcpy(cwd, "/");
        }
        root = cwd;
    }

    if (opts.type != TYPE_FILE) {
        if (!opts.name_pattern || glob_match(opts.name_pattern, root)) {
            printf("%s\n", root);
        }
    }

    return do_find(root, 0, &opts);
}