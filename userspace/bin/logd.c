// logd.c
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 01.10.25.
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

#include <channel.h>
#include <dirent.h>
#include <fflags.h>
#include <stdio.h>
#include <string.h>

#define LOG_DEVICE "/dev/log"
#define LOG_FILE   "/var/log/kernel.log"
#define BUFFER_SIZE 512


int main() {
    HANDLE log_fd = open(LOG_DEVICE, O_RDWR);
    if (log_fd < 0) {
        printf("Failed to open /dev/log: %ld", log_fd);
        return 1;
    }

    char* buf = "LogD initialized. starting logger...";
    write(log_fd, buf, strlen(buf));

    create(LOG_FILE, C_FILE);

    FILE_HANDLE log_file = open(LOG_FILE, O_WRONLY);
    if (!log_file) {
        puts("Failed to open log file");
        close(log_fd);
        return 1;
    }

    char buffer[BUFFER_SIZE];

    while (1) {
        const ssize_t n = read(log_fd, buffer, sizeof(buffer) - 1);
        if (n < 0) {
            puts("Error reading from /dev/log");
            break;
        } else if (n == 0) {
            continue;
        }

        buffer[n] = '\0';
        write(log_file, buffer, n);
        break;
    }


    close(log_file);
    close(log_fd);

    char* path = "/var/log";
    FILE_HANDLE hdl = open(path, O_RDONLY);
    if (hdl < 0) {
        if (hdl == -2) {
            printf("ls: Cannot open '%s': File or directory not found\n", path);
            return 1;
        }
        printf("ls: Cannot open '%s' due to an unknown error\n", path);
        return 1;
    }

    dirent_t ent;
    while (sys_readdir(hdl, (uint64_t)&ent, sizeof(ent), 0, 0, 0) > 0) {
        const char *color = "\033[0m"; // reset
        switch (ent.type) {
            case DT_DIR:    color = "\033[38;2;66;117;245m"; break; // blau
            case DT_EXEC:   color = "\033[38;2;66;245;81"; break; // grün
            case DT_SYMLINK:color = "\033[1;36m"; break; // cyan
            case DT_CHARDEV:
            case DT_BLOCKDEV: color = "\033[38;2;245;212;66m"; break;
            default:        color = "\033[0m";    break;
        }
        printf("%s%s\033[0m ", color, ent.name);
    }

    putchar('\n');
    close(hdl);

    return 0;
}
