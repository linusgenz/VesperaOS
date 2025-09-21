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

#define HANDLE_TYPE_MASK    0xFFFF000000000000ULL
#define HANDLE_ID_MASK      0x0000FFFFFFFFFFFFULL

#define HANDLE_TYPE_CONSOLE 0x1000000000000000ULL
#define HANDLE_TYPE_FILE    0x2000000000000000ULL
#define HANDLE_TYPE_CHANNEL 0x3000000000000000ULL
#define HANDLE_TYPE_UNIT    0x4000000000000000ULL
#define HANDLE_TYPE_REALM   0x5000000000000000ULL
#define HANDLE_TYPE_DEVICE  0x6000000000000000ULL

#define HANDLE_STDIN   (HANDLE_TYPE_CONSOLE | 0x0000000000000000ULL)  // Console, Slot 0
#define HANDLE_STDOUT  (HANDLE_TYPE_CONSOLE | 0x0000000000000001ULL)  // Console, Slot 1
#define HANDLE_STDERR  (HANDLE_TYPE_CONSOLE | 0x0000000000000002ULL)
#define MAX_INPUT 256
#define MAX_ARGS 16
#define MAX_PATH 256
#define HISTORY_SIZE 32

#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040

#include "stddef.h"
#include "stdint.h"

// System call numbers
#define SYS_READ    0
#define SYS_WRITE   1
#define SYS_OPEN    2
#define SYS_CLOSE   3
#define SYS_STAT    4
#define SYS_IOCTL   16
#define SYS_GETPID  39
#define SYS_EXIT    60
#define SYS_GETCWD  79
#define SYS_CHDIR   80

typedef struct {
    char *args[MAX_ARGS];
    int argc;
} command_t;

static char history[HISTORY_SIZE][MAX_INPUT];
static int history_count = 0;
static int history_index = 0;
static char current_dir[MAX_PATH] = "/";

int64_t syscall(
    uint64_t num,
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    int64_t ret = -1;

    register uint64_t r10_ asm("r10") = arg3;
    register uint64_t r8_ asm("r8") = arg4;
    register uint64_t r9_ asm("r9") = arg5;

    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg0), "S"(arg1), "d"(arg2),
        "r"(r10_), "r"(r8_), "r"(r9_)
        : "rcx", "r11", "memory"
    );

    return ret;
}

long sys_ioctl(uint64_t fd, uint32_t cmd, void* arg) {
    return syscall(SYS_IOCTL, fd, cmd, (long)arg, 0, 0,0 );
}


static int64_t sys_read(int64_t fd, void *buf, uint64_t count) {
    return syscall(SYS_READ, fd, (long) buf, count, 0, 0, 0);
}

static int64_t sys_write(int64_t fd, const void *buf, uint64_t count) {
    return syscall(SYS_WRITE, fd, (long) buf, count, 0, 0, 0);
}

static int64_t sys_getcwd(char *buf, uint64_t size) {
    return syscall(SYS_GETCWD, (long) buf, size, 0, 0, 0, 0);
}

static int64_t sys_chdir(const char *path) {
    return syscall(SYS_CHDIR, (long) path, 0, 0, 0, 0, 0);
}

static int64_t sys_getpid(void) {
    return syscall(SYS_GETPID, 0, 0, 0, 0, 0, 0);
}

size_t strlen(const char *s) {
    const char *start = s;
    while (*s) {
        ++s;
    }
    return s - start;
}

void print(const char *msg) {
    if (!msg) return;
    int len = strlen(msg);
    sys_write(HANDLE_STDOUT, msg, len);
}

void print_error(const char *msg) {
    if (!msg) return;
    int len = strlen(msg);
    sys_write(HANDLE_STDERR, msg, len);
}

void print_int(int64_t num) {
    if (num == 0) {
        print("0");
        return;
    }

    char buf[32];
    int i = 0;
    int negative = 0;

    if (num < 0) {
        negative = 1;
        num = -num;
    }

    while (num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }

    if (negative) print("-");

    for (int j = i - 1; j >= 0; j--) {
        char c = buf[j];
        sys_write(HANDLE_STDOUT, &c, 1);
    }
}

int strncmp(const char *a, const char *b, int max) {
    for (int i = 0; i < max; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

int strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

void strcpy(char *dest, const char *src) {
    if (!dest || !src) return;
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

void strcat(char *dest, const char *src) {
    if (!dest || !src) return;
    while (*dest) dest++;
    strcpy(dest, src);
}

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
    print("VesperaOS Shell v1.1\n");
    print("Available commands:\n");
    print("  help      - Show this help message\n");
    print("  hello     - Print greeting\n");
    print("  echo      - Echo arguments\n");
    print("  pwd       - Print working directory\n");
    print("  cd        - Change directory\n");
    print("  pid       - Show process ID\n");
    print("  history   - Show command history\n");
    print("  clear     - Clear screen\n");
    print("  exit      - Exit shell\n");
}

void cmd_hello(command_t *cmd) {
    if (cmd->argc > 1) {
        print("Hello, ");
        print(cmd->args[1]);
        print("!\n");
    } else {
        print("Hello, world!\n");
    }
}

void cmd_echo(command_t *cmd) {
    for (int i = 1; i < cmd->argc; i++) {
        if (i > 1) print(" ");
        print(cmd->args[i]);
    }
    print("\n");
}

void cmd_pwd(command_t *cmd) {
    char cwd[MAX_PATH];
    if (sys_getcwd(cwd, sizeof(cwd)) >= 0) {
        print(cwd);
        print("\n");
    } else {
        print(current_dir);
        print("\n");
    }
}

void cmd_cd(command_t *cmd) {
    const char *path = (cmd->argc > 1) ? cmd->args[1] : "/";

    if (sys_chdir(path) == 0) {
        strcpy(current_dir, path);
    } else {
        print_error("cd: cannot change directory to '");
        print_error(path);
        print_error("'\n");
    }
}

void cmd_pid(command_t *cmd) {
    int64_t pid = sys_getpid();
    print("PID: ");
    print_int(pid);
    print("\n");
}

void cmd_history(command_t *cmd) {
    int start = (history_count > HISTORY_SIZE) ? history_count - HISTORY_SIZE : 0;
    int end = history_count;

    for (int i = start; i < end; i++) {
        print_int(i + 1);
        print(": ");
        print(history[i % HISTORY_SIZE]);
        print("\n");
    }
}

void cmd_clear(command_t *cmd) {
    print("\033[2J\033[H"); // ANSI escape codes
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
    } else if (strcmp(command, "pid") == 0) {
        cmd_pid(cmd);
    } else if (strcmp(command, "history") == 0) {
        cmd_history(cmd);
    } else if (strcmp(command, "clear") == 0) {
        cmd_clear(cmd);
    } else if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
        return -1; // Signal to exit
    } else {
        print_error("Unknown command: ");
        print_error(command);
        print_error("\n");
        print_error("Type 'help' for available commands.\n");
    }

    return 0;
}


void show_prompt(void) {
    print("[VesperaOS:");

    // Show current directory (basename only)
    const char *dir = current_dir;
    const char *last_slash = nullptr;
    while (*dir) {
        if (*dir == '/') last_slash = dir;
        dir++;
    }

    if (last_slash && *(last_slash + 1)) {
        print(last_slash + 1);
    } else {
        print("/");
    }

    print("]$ ");
}

typedef struct {
    uint8_t slot_id;
    uint8_t port_num;
    uint8_t speed;
    uint8_t bus_number;
    uint16_t vendor_id;
    uint16_t product_id;
    char product[64];
    char manufacturer[64];
    char serial_number[64];
} xhci_device_stat;

void memset(void* dest, uint8_t val, uint64_t num) {
    for (uint64_t i = 0; i < num; i++) {
        *(uint8_t*)((uint64_t)dest + i) = val;
    }
}

void shell_main() {
    char buf[MAX_INPUT];
    command_t cmd;

    // Clear screen and show welcome
    cmd_clear(nullptr);
    print("Welcome to VesperaOS Shell!\n");
    print("Type 'help' for available commands.\n\n");

    // Get initial working directory
    sys_getcwd(current_dir, sizeof(current_dir));

    auto handleID = syscall(SYS_OPEN, (long) "/dev/xhci1/", O_RDONLY, 0, 0, 0, 0);
    print("Handle ID: ");
    print_int(handleID);

    size_t devices = 0;
    memset(&devices, 0, sizeof(devices));

    long ret = sys_ioctl(handleID, 1, &devices);
    if (ret < 0) {
        print("error: ioctl");
        print_int(ret);
    //    close(fd);
    while (1);
    }

    print("Xchi devices: ");
    print_int(devices);


    char buffer[256];
    size_t bytes;
    while (true) {
        bytes = syscall(SYS_READ, handleID, (long) buffer, sizeof(buffer), 0, 0, 0);
        if (bytes == 0) break;

        size_t entries = bytes / sizeof(xhci_device_stat);
        xhci_device_stat *stats = (xhci_device_stat *) buffer;

        for (size_t i = 0; i < entries; i++) {
            print("Bus ");
            print_int(stats[i].bus_number);
            print(", Slot ");
            print_int(stats[i].slot_id);
            print(", Port ");
            print_int(stats[i].port_num);
            print(", Speed ");
            print_int(stats[i].speed);
            print(" ID ");
            print_int(stats[i].vendor_id);
            print("\n");
        }
    }

    while (1) {
        show_prompt();

        int n = sys_read(HANDLE_STDIN, buf, MAX_INPUT - 1);
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
                print("Goodbye!\n");
                break;
            }
        }
    }
}

void _start() {
    shell_main();
    syscall(SYS_EXIT, 0, 0, 0, 0, 0, 0);
}
