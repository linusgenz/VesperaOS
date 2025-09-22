// shell.c
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 05.08.25.
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

#define MAX_INPUT 256
#define MAX_ARGS 16
#define MAX_PATH 256
#define HISTORY_SIZE 32

#include <string.h>
#include <stdio.h>
#include <fflags.h>
#include "stddef.h"
#include "stdint.h"
#include <realm.h>
#include <sys/ioctl.h>
#include <dev/usb_xhci_ioctl.h>


typedef struct {
    char *args[MAX_ARGS];
    int argc;
} command_t;

static char history[HISTORY_SIZE][MAX_INPUT];
static int history_count = 0;
static int history_index = 0;
static char current_dir[MAX_PATH] = "/";

char *trim_whitespace(char *str) {
    if (!str) return nullptr;

    // Skip leading whitespace
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }

    // Find end
    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }

    return str;
}

void add_to_history(const char *cmd) {
    if (!cmd || strlen(cmd) == 0) return;

    strcpy(history[history_count % HISTORY_SIZE], cmd);
    history_count++;
    history_index = history_count;
}

int parse_command(char *input, command_t *cmd) {
    if (!input || !cmd) return -1;

    cmd->argc = 0;
    char *token = input;

    while (*token && cmd->argc < MAX_ARGS - 1) {
        // Skip whitespace
        while (*token == ' ' || *token == '\t') token++;
        if (*token == '\0') break;

        cmd->args[cmd->argc++] = token;

        // Find end of token
        while (*token && *token != ' ' && *token != '\t') token++;
        if (*token) *token++ = '\0';
    }

    cmd->args[cmd->argc] = nullptr;
    return cmd->argc;
}

void cmd_help(command_t *cmd) {
    printf("VesperaOS Shell v1.1\n");
    printf("Available commands:\n");
    printf("  help      - Show this help message\n");
    printf("  hello     - Print greeting\n");
    printf("  echo      - Echo arguments\n");
    printf("  pwd       - Print working directory\n");
    printf("  cd        - Change directory\n");
    printf("  pid       - Show process ID\n");
    printf("  history   - Show command history\n");
    printf("  clear     - Clear screen\n");
    printf("  exit      - Exit shell\n");
}

void cmd_hello(command_t *cmd) {
    if (cmd->argc > 1) {
        printf("Hello, ");
        printf(cmd->args[1]);
        printf("!\n");
    } else {
        printf("Hello, world!\n");
    }
}

void cmd_echo(command_t *cmd) {
    for (int i = 1; i < cmd->argc; i++) {
        if (i > 1) printf(" ");
        printf(cmd->args[i]);
    }
    printf("\n");
}

void cmd_pwd(command_t *cmd) {
    char cwd[MAX_PATH];
    /*if (sys_getcwd(cwd, sizeof(cwd)) >= 0) {
        printf(cwd);
        printf("\n");
    } else {
        printf(current_dir);
        printf("\n");
    }*/
}

void cmd_cd(command_t *cmd) {
    const char *path = (cmd->argc > 1) ? cmd->args[1] : "/";

    /* if (sys_chdir(path) == 0) {
         strcpy(current_dir, path);
     } else {
         printf("cd: cannot change directory to '%s'\n", path);
     }*/
}


void cmd_history(command_t *cmd) {
    int start = (history_count > HISTORY_SIZE) ? history_count - HISTORY_SIZE : 0;
    int end = history_count;

    for (int i = start; i < end; i++) {
        printf("%d: %s\n", i + 1, history[i % HISTORY_SIZE]);
    }
}

void cmd_clear(command_t *cmd) {
    printf("\033[2J\033[H"); // ANSI escape codes
}

int execute_command(command_t *cmd) {
    if (cmd->argc == 0) return 0;

    const char *command = cmd->args[0];

    if (strcmp(command, "help") == 0) {
        cmd_help(cmd);
    } else if (strcmp(command, "hello") == 0) {
        cmd_hello(cmd);
    } else if (strcmp(command, "echo") == 0) {
        cmd_echo(cmd);
    } else if (strcmp(command, "pwd") == 0) {
        cmd_pwd(cmd);
    } else if (strcmp(command, "cd") == 0) {
        cmd_cd(cmd);
    } else if (strcmp(command, "history") == 0) {
        cmd_history(cmd);
    } else if (strcmp(command, "clear") == 0) {
        cmd_clear(cmd);
    } else if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
        return -1; // Signal to exit
    } else {
        printf("Unknown command: ");
        printf(command);
        printf("\n");
        printf("Type 'help' for available commands.\n");
    }

    return 0;
}


void show_prompt(void) {
    printf("[VesperaOS:");

    // Show current directory (basename only)
    const char *dir = current_dir;
    const char *last_slash = nullptr;
    while (*dir) {
        if (*dir == '/') last_slash = dir;
        dir++;
    }

    if (last_slash && *(last_slash + 1)) {
        printf(last_slash + 1);
    } else {
        printf("/");
    }

    printf("]$ ");
}

void shell_main() {
    char buf[MAX_INPUT];
    command_t cmd;

    // Clear screen and show welcome
    cmd_clear(nullptr);
    printf("Welcome to VesperaOS Shell!\n");
    printf("Type 'help' for available commands.\n\n");


    auto handleID = fopen("/dev/xhci1/", O_RDONLY);
    printf("Handle ID: %u", handleID);

    size_t devices = 0;
    memset(&devices, 0, sizeof(devices));

    long ret = ioctl(handleID, XHCI_IOCTL_GET_COUNT, &devices);
    if (ret < 0) {
        printf("error: ioctl: %lld", ret);
        //    close(fd);
    }
    const char *argv[] = {"lsusb", NULL};
    auto rid = spawn_realm("/mnt/fat32_0/bin/lsusb.elf", 1, argv);
    printf("spawn rid: %d", rid);

    while (1) {
        show_prompt();

        int n = fread(HANDLE_STDIN, buf, MAX_INPUT - 1);
        if (n <= 0) continue;

        buf[n] = '\0';

        // Strip newline
        if (n > 0 && buf[n - 1] == '\n') {
            buf[n - 1] = '\0';
            n--;
        }

        // Skip empty lines
        char *trimmed = trim_whitespace(buf);
        if (strlen(trimmed) == 0) continue;

        // Add to history
        add_to_history(trimmed);

        // Parse and execute command
        if (parse_command(trimmed, &cmd) > 0) {
            if (execute_command(&cmd) < 0) {
                printf("Goodbye!\n");
                break;
            }
        }
    }
}

void _start() {
    shell_main();
    exit(0);
}
