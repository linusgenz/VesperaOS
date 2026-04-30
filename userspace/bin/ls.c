// ls.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 23.03.26.
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
#include <fflags.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysstd.h>
#include <time.h>
#include <vespera/stat.h>

#define MAX_PATH 512

static void usage(void) {
    puts("Usage: ls [OPTION]... [DIRECTORY]...");
    puts("List directory contents.");
    puts("");
    puts("Options:");
    puts("  -l    use long format (show permissions, size)");
    puts("  -a    show hidden files (starting with '.')");
    puts("  -F    append indicator (one of */@) to entries");
    puts("  --help     display this help and exit");
}

static void format_size(uint64_t size, char* buf, size_t buf_size) {
    if (size >= 1024 * 1024 * 1024)
        snprintf(buf, buf_size, "%6.1fG", (double)size / (1024 * 1024 * 1024));
    else if (size >= 1024 * 1024)
        snprintf(buf, buf_size, "%6.1fM", (double)size / (1024 * 1024));
    else if (size >= 1024)
        snprintf(buf, buf_size, "%6.1fK", (double)size / 1024);
    else
        snprintf(buf, buf_size, "%6lluB", (unsigned long long)size);
}

static void format_mode(uint16_t mode, char* buf) {
    // Type
    switch (mode & 0xF000) {
        case 0x4000: buf[0] = 'd'; break;
        case 0x8000: buf[0] = '-'; break;
        case 0xA000: buf[0] = 'l'; break;
        case 0x2000: buf[0] = 'c'; break;
        case 0x6000: buf[0] = 'b'; break;
        default:     buf[0] = '?'; break;
    }

    const char bits[] = "rwxrwxrwx";

    for (int i = 0; i < 9; i++) {
        buf[i + 1] = (mode & (1 << (8 - i))) ? bits[i] : '-';
    }

    buf[10] = '\0';
}

static int list_dir(const char* path, int long_fmt, int classify, int show_all) {
    DIR_HANDLE hdl = opendir(path);
    if ((int64_t)hdl < 0) {
        if (hdl == -ENOENT)
            printf("ls: cannot access '%s': No such file or directory\n", path);
        else
            printf("ls: cannot open '%s': %s\n", path, strerror((int)hdl));
        return 1;
    }

    char cwd[MAX_PATH];
    if (getcwd(cwd, sizeof(cwd)) < 0) {
        cwd[0] = '/';
        cwd[1] = '\0';
    }

    dirent_t ent;
    while (readdir(hdl, &ent) > 0) {
        // Skip hidden files unless -a
        if (!show_all && ent.name[0] == '.') continue;

        const char* color = "\033[0m";
        const char* indicator = "";

        switch (ent.type) {
            case DT_DIR:
                color = "\033[38;2;66;117;245m";
                if (classify) indicator = "/";
                break;
            case DT_EXEC:
                color = "\033[38;2;66;245;81m";
                if (classify) indicator = "*";
                break;
            case DT_SYMLINK:
                color = "\033[1;36m";
                if (classify) indicator = "@";
                break;
            case DT_BLOCKDEV:
                color = "\033[38;2;100;200;255m";
                break;
            case DT_CHARDEV:
                color = "\033[38;2;245;212;8m";
                break;
            default:
                break;
        }

        if (!long_fmt) {
            printf("%s%s%s\033[0m ", color, ent.name, indicator);
            continue;
        }

        // Long format: stat for size and flags
        char full_path[MAX_PATH];
        if (strcmp(path, "/") == 0)
            snprintf(full_path, sizeof(full_path), "/%s", ent.name);
        else
            snprintf(full_path, sizeof(full_path), "%s/%s", path, ent.name);

        vespera_stat_t st;
        int has_stat = (sys_stat((uint64_t)full_path, (uint64_t)&st, 0, 0, 0, 0) == 0);

        char type_char = '-';
        switch (ent.type) {
            case DT_DIR:      type_char = 'd'; break;
            case DT_CHARDEV:  type_char = 'c'; break;
            case DT_BLOCKDEV: type_char = 'b'; break;
            case DT_SYMLINK:  type_char = 'l'; break;
            default:          type_char = '-'; break;
        }

        char mode_buf[11] = "----------";
        if (has_stat) {
            format_mode(st.mode, mode_buf);
        }


        char size_buf[16] = "     -";
        if (has_stat)
            format_size(st.size, size_buf, sizeof(size_buf));

        char time_buf[32] = "???";

        if (has_stat) {
            strftime_unix(time_buf, sizeof(time_buf), "%b %e %H:%M", st.mtime);
        }

        u32 links = has_stat ? st.links_count : 1;

        printf("%s %2u %6s %12s %s%s%s\033[0m\n",
               mode_buf,
               links,
               size_buf,
               time_buf,
               color,
               ent.name,
               indicator);
    }

    if (!long_fmt) putchar('\n');

    closedir(hdl);
    return 0;
}

int main(int argc, char** argv) {
    int long_fmt = 0;
    int classify = 0;
    int show_all = 0;
    const char* path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        }
        if (argv[i][0] == '-') {
            for (int j = 1; argv[i][j]; j++) {
                switch (argv[i][j]) {
                    case 'l': long_fmt = 1; break;
                    case 'F': classify = 1; break;
                    case 'a': show_all = 1; break;
                    default:
                        printf("ls: invalid option -- '%c'\n", argv[i][j]);
                        return 1;
                }
            }
        } else {
            path = argv[i];
        }
    }

    if (!path) {
        char cwd[MAX_PATH];
        if (getcwd(cwd, sizeof(cwd)) < 0) {
            strcpy(cwd, "/");
        }
        path = cwd;
    }

    return list_dir(path, long_fmt, classify, show_all);
}