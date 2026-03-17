// stat.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.03.26.
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vespera/stat.h>

#define C_RESET  "\033[0m"
#define C_BOLD   "\033[1m"
#define C_DIM    "\033[2m"
#define C_CYAN   "\033[36m"
#define C_YELLOW "\033[33m"
#define C_GREEN  "\033[32m"
#define C_RED    "\033[31m"
#define C_BLUE   "\033[34m"

static const char* type_name(uint8_t t) {
    switch (t) {
        case VSTAT_TYPE_FILE:     return "regular file";
        case VSTAT_TYPE_DIR:      return "directory";
        case VSTAT_TYPE_CHARDEV:  return "character device";
        case VSTAT_TYPE_BLOCKDEV: return "block device";
        case VSTAT_TYPE_SYMLINK:  return "symbolic link";
        default:                  return "unknown";
    }
}

static const char* type_color(uint8_t t) {
    switch (t) {
        case VSTAT_TYPE_DIR:      return "\033[38;2;66;117;245m";
        case VSTAT_TYPE_CHARDEV:  return "\033[38;2;245;212;8m";
        case VSTAT_TYPE_BLOCKDEV: return "\033[38;2;100;200;255m";
        case VSTAT_TYPE_SYMLINK:  return "\033[1;36m";
        default:                  return C_RESET;
    }
}

static void format_size_human(uint64_t size, char* buf, size_t n) {
    if (size >= 1024ULL * 1024 * 1024)
        snprintf(buf, n, "%.2f GiB", (double)size / (1024.0*1024*1024));
    else if (size >= 1024 * 1024)
        snprintf(buf, n, "%.2f MiB", (double)size / (1024.0*1024));
    else if (size >= 1024)
        snprintf(buf, n, "%.2f KiB", (double)size / 1024.0);
    else
        snprintf(buf, n, "%llu bytes", (unsigned long long)size);
}

static void print_flags(uint32_t flags) {
    char buf[128] = {};
    int first = 1;

    struct { uint32_t bit; const char* name; } table[] = {
        { VSTAT_FLAG_READABLE,  "readable"  },
        { VSTAT_FLAG_WRITABLE,  "writable"  },
        { VSTAT_FLAG_EXEC,      "exec"      },
        { VSTAT_FLAG_VIRTUAL,   "virtual"   },
        { VSTAT_FLAG_PERMANENT, "permanent" },
    };

    for (int i = 0; i < 5; i++) {
        if (flags & table[i].bit) {
            if (!first) strncat(buf, ", ", sizeof(buf) - strlen(buf) - 1);
            strncat(buf, table[i].name, sizeof(buf) - strlen(buf) - 1);
            first = 0;
        }
    }

    if (first) strncat(buf, "none", sizeof(buf) - strlen(buf) - 1);
    printf("  %-18s%s\n", "Flags:", buf);
}

static void show_stat(const char* path) {
    vespera_stat_t st;
    int64_t rc = sys_stat((uint64_t)path, (uint64_t)&st, 0, 0, 0, 0);

    if (rc < 0) {
        switch ((int)rc) {
            case -2:  printf("stat: '%s': No such file or directory\n", path); break;
            case -14: printf("stat: '%s': Bad address\n", path);               break;
            default:  printf("stat: '%s': Error %lld\n", path, rc);            break;
        }
        return;
    }

    const char* tcolor = type_color(st.node_type);
    char human[32];
    format_size_human(st.size, human, sizeof(human));

    printf("  %s%s%s\n\n", C_BOLD, path, C_RESET);
    printf("  %-18s%s%s%s\n",   "Type:",    tcolor, type_name(st.node_type), C_RESET);
    printf("  %-18s%llu bytes", "Size:",    (unsigned long long)st.size);

    if (st.size >= 1024)
        printf("  %s(%s)%s", C_DIM, human, C_RESET);
    printf("\n");

    if (st.blocks > 0) {
        printf("  %-18s%llu",    "Blocks:",     (unsigned long long)st.blocks);
        printf("  %s(block size: %u bytes)%s\n", C_DIM, st.block_size, C_RESET);
    }

    if (st.inode_id != 0)
        printf("  %-18s%s0x%08llX%s\n", "Inode:",
               C_YELLOW, (unsigned long long)st.inode_id, C_RESET);

    if (st.dev_id != 0)
        printf("  %-18s%u\n", "Device ID:", st.dev_id);

    print_flags(st.flags);
}

static void usage(void) {
    printf("Usage: stat <path> [path...]\n");
    printf("       stat --help\n");
}

int main(int argc, const char** argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage();
        return 0;
    }

    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (i > 1) printf("\n");
        show_stat(argv[i]);
    }
    return rc;
}