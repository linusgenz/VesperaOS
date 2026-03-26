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

#include <errno.h>
#include <exec.h>
#include <readline.h>
#include <realm.h>
#include <reboot.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vespera/fflags.h>

typedef struct command {
    char* args[MAX_ARGS];
    int argc;
    char* input_file;
    char* output_file;
    bool append_output;
    bool has_pipe;         // true if this command is followed by '|'
    struct command* next;  // next command in pipeline (if has_pipe)
} command_t;

static char history[HISTORY_SIZE][MAX_INPUT];
static int history_count = 0;
static int history_index = 0;
static char current_dir[MAX_PATH] = "/";

char* trim_whitespace(char* str) {
    if (!str) return NULL;

    // Skip leading whitespace
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }

    // Find end
    char* end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }

    return str;
}

void add_to_history(const char* cmd) {
    if (!cmd || strlen(cmd) == 0) return;

    strcpy(history[history_count % HISTORY_SIZE], cmd);
    history_count++;
    history_index = history_count;
}

// Forward declaration
static int parse_single_command(char* input, command_t* cmd);

int parse_command(char* input, command_t* cmd) {
    if (!input || !cmd) return -1;

    // Initialize first command
    cmd->has_pipe = false;
    cmd->next = NULL;

    char* pipe_pos = NULL;
    int depth = 0;
    char* scan = input;

    // Find first '|' at depth 0 (not inside redirects)
    while (*scan) {
        if (*scan == '<' || *scan == '>') {
            depth++;
            scan++;
            continue;
        }
        if (depth > 0 && (*scan == ' ' || *scan == '\t')) {
            depth--;
            scan++;
            continue;
        }
        if (*scan == '|' && depth == 0) {
            pipe_pos = scan;
            break;
        }
        scan++;
    }

    if (pipe_pos) {
        // Split at pipe
        *pipe_pos = '\0';
        cmd->has_pipe = true;
        cmd->next = (command_t*)malloc(sizeof(command_t));
        if (!cmd->next) return -1;
        memset(cmd->next, 0, sizeof(command_t));

        // Parse this command
        parse_single_command(input, cmd);

        // Parse next command
        return parse_command(pipe_pos + 1, cmd->next);
    }

    return parse_single_command(input, cmd);
}

static int parse_single_command(char* input, command_t* cmd) {
    if (!input || !cmd) return -1;

    cmd->argc = 0;
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->append_output = false;
    cmd->has_pipe = false;
    cmd->next = NULL;

    char* token = input;

    while (*token && cmd->argc < MAX_ARGS - 1) {
        // Skip whitespace
        while (*token == ' ' || *token == '\t') token++;
        if (*token == '\0') break;

        if (*token == '<') {
            // Input redirect
            token++;
            while (*token == ' ' || *token == '\t') token++;
            cmd->input_file = token;

            while (*token && *token != ' ' && *token != '\t' && *token != '>' && *token != '<') token++;
            if (*token) *token++ = '\0';
            continue;
        }

        if (*token == '>') {
            // Output redirect
            token++;

            if (*token == '>') {
                cmd->append_output = true;
                token++;
            }

            while (*token == ' ' || *token == '\t') token++;
            cmd->output_file = token;

            while (*token && *token != ' ' && *token != '\t' && *token != '>' && *token != '<') token++;
            if (*token) *token++ = '\0';
            continue;
        }

        if (*token == '"') {
            token++;
            cmd->args[cmd->argc++] = token;

            while (*token && *token != '"') token++;

            if (*token == '"') {
                *token = '\0';
                token++;
            }
        } else {
            cmd->args[cmd->argc++] = token;

            while (*token && *token != ' ' && *token != '\t') token++;

            if (*token) *token++ = '\0';
        }
    }

    cmd->args[cmd->argc] = NULL;
    return cmd->argc;
}

void cmd_help(command_t* cmd) {
    (void)cmd;
    printf("\033[38;2;66;117;245m\uf489  VesperaOS Shell v2.1\033[0m\n\n");
    printf("\033[38;2;180;180;180mBuilt-in commands:\033[0m\n\n");

    printf("  \033[38;2;66;245;81m\uf002\033[0m  help      - Show this help\n");
    printf("  \033[38;2;66;245;81m\uf015\033[0m  cd        - Change directory\n");
    printf("  \033[38;2;66;245;81m\uf0ac\033[0m  pwd       - Print working directory\n");
    printf("  \033[38;2;66;245;81m\uf0e0\033[0m  echo      - Print arguments\n");
    printf("  \033[38;2;66;245;81m\uf1da\033[0m  history   - Command history\n");
    printf("  \033[38;2;66;245;81m\uf12d\033[0m  clear     - Clear screen\n");
    printf("  \033[38;2;245;100;80m\uf011\033[0m  shutdown  - Shutdown the PC\n");
    printf("  \033[38;2;245;100;80m\uf0e2\033[0m  reboot    - Reboot the PC\n");

    printf("\n\033[38;2;180;180;180mExternal commands:\033[0m\n\n");
    printf("  ls, cat, cp, mv, rm, mkdir, rmdir, touch, sleep\n");
    printf("  stat, mount, umount, memstat, uptime, lsusb, diskinfo\n");

    printf("\n\033[38;2;180;180;180mRedirection & Pipes:\033[0m\n");
    printf("  < file     - Redirect stdin from file\n");
    printf("  > file     - Redirect stdout to file (truncate)\n");
    printf("  >> file    - Redirect stdout to file (append)\n");
    printf("  | cmd      - Pipe stdout to next command's stdin\n");
    printf("\n  Example: ls -l | grep '.c' > files.txt\n");
}

void cmd_hello(command_t* cmd) {
    if (cmd->argc > 1) {
        printf("Hello, ");
        printf(cmd->args[1]);
        printf("!\n");
    } else {
        printf("Hello, world!\n");
    }
}

static const char* expand_var(const char* input) {
    if (input[0] == '$') {
        const char* val = getenv(input + 1);
        if (val) return val;
        return "";
    }
    return input;
}

void cmd_echo(command_t* cmd) {
    for (int i = 1; i < cmd->argc; i++) {
        if (i > 1) printf(" ");

        const char* expanded = expand_var(cmd->args[i]);
        printf("%s", expanded);
    }
    printf("\n");
}

void cmd_cd(command_t* cmd) {
    const char* path = (cmd->argc > 1) ? cmd->args[1] : "/";

    if (chdir(path) == 0) {
        if (!getcwd(current_dir, sizeof(current_dir))) {
            strcpy(current_dir, "/");
        }
    } else {
        printf("cd: cannot change directory to '%s'\n", path);
    }
}

void cmd_history(command_t* cmd) {
    (void)cmd;
    int start = (history_count > HISTORY_SIZE) ? history_count - HISTORY_SIZE : 0;
    int end = history_count;

    for (int i = start; i < end; i++) {
        printf("%d: %s\n", i + 1, history[i % HISTORY_SIZE]);
    }
}

void cmd_clear(command_t* cmd) {
    (void)cmd;
    printf("\033[2J\033[H");  // ANSI escape codes
}

// Execute a pipeline of commands
static int execute_pipeline(command_t* head) {
    if (!head) return 0;

    command_t* current = head;
    FILE_HANDLE pipe_read = -1;
    FILE_HANDLE pipe_write = -1;
    int result = 0;

    while (current) {
        command_t* next = current->next;
        FILE_HANDLE saved_stdin = stdin;
        FILE_HANDLE saved_stdout = stdout;
        FILE_HANDLE redirect_in = -1;
        FILE_HANDLE redirect_out = -1;

        // If we have a previous pipe, set up read end as stdin
        if (pipe_read >= 0) {
            stdin = pipe_read;
            pipe_read = -1;  // consumed
        }

        // If this command feeds into next, create pipe
        if (next && current->has_pipe) {
            int fds[2];
            if (sys_pipe((uint64_t)fds, 0, 0, 0, 0, 0) < 0) {
                printf("pipe: creation failed\n");
                if (pipe_read >= 0) close(pipe_read);
                if (pipe_write >= 0) close(pipe_write);
                return -1;
            }
            pipe_read = fds[0];
            pipe_write = fds[1];
            // Wire up write end to stdout
            redirect_out = pipe_write;
            stdout = redirect_out;
        }

        // Handle file redirects
        if (current->input_file && pipe_read < 0) {
            redirect_in = open(current->input_file, O_RDONLY);
            if (redirect_in < 0) {
                printf("Cannot open input file '%s'\n", current->input_file);
                if (pipe_write >= 0) close(pipe_write);
                stdin = saved_stdin;
                current = next;
                continue;
            }
            stdin = redirect_in;
        }

        if (current->output_file && !pipe_write) {
            int flags = O_WRONLY | O_CREAT;
            if (current->append_output) {
                flags |= O_APPEND;
            } else {
                flags |= O_TRUNC;
            }

            redirect_out = open(current->output_file, flags);
            if (redirect_out < 0) {
                printf("Cannot open output file '%s'\n", current->output_file);
                if (redirect_in >= 0) {
                    close(redirect_in);
                    stdin = saved_stdin;
                }
                if (pipe_write >= 0) close(pipe_write);
                current = next;
                continue;
            }
            stdout = redirect_out;
        }

        // Execute the command
        const char* command = current->args[0];

        // Built-in commands (only those that must be built-in)
        if (strcmp(command, "help") == 0) {
            cmd_help(current);
        } else if (strcmp(command, "hello") == 0) {
            cmd_hello(current);
        } else if (strcmp(command, "echo") == 0) {
            cmd_echo(current);
        } else if (strcmp(command, "cd") == 0) {
            cmd_cd(current);
        } else if (strcmp(command, "history") == 0) {
            cmd_history(current);
        } else if (strcmp(command, "clear") == 0) {
            cmd_clear(current);
        } else if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
            result = -1;
        } else {
            // External command
            const char* prog = find_executable(command);
            if (prog) {
                int64_t rid = spawn_realm(prog, current->args, environ, NULL);
                if (rid < 0) {
                    printf("spawn failed: %s\n", strerror(rid));
                } else {
                    int status = 0;
                    wait_realm(rid, &status);
                    if (status != 0 && result >= 0) {
                        result = status;
                    }
                }
            } else {
                printf("Unknown command: %s\n", command);
                printf("Type 'help' for available commands.\n");
            }
        }

        // Restore stdio
        if (redirect_in >= 0) {
            close(redirect_in);
            stdin = saved_stdin;
        }

        if (redirect_out >= 0 && redirect_out != pipe_write) {
            close(redirect_out);
            stdout = saved_stdout;
        }

        // Close write end after command completes
        if (pipe_write >= 0) {
            close(pipe_write);
            pipe_write = -1;
        }

        // Move to next command
        current = next;
    }

    // Cleanup any remaining pipe fds
    if (pipe_read >= 0) close(pipe_read);
    if (pipe_write >= 0) close(pipe_write);

    return result;
}

int execute_command(command_t* cmd) {
    if (cmd->argc == 0) return 0;

    // Check if this is a pipeline
    if (cmd->has_pipe || cmd->next) {
        return execute_pipeline(cmd);
    }

    // Single command - use original logic
    FILE_HANDLE saved_stdin = stdin;
    FILE_HANDLE saved_stdout = stdout;
    FILE_HANDLE redirect_in = -1;
    FILE_HANDLE redirect_out = -1;

    if (cmd->input_file) {
        redirect_in = open(cmd->input_file, O_RDONLY);
        if (redirect_in < 0) {
            printf("Cannot open input file '%s'\n", cmd->input_file);
            return 0;
        }
        stdin = redirect_in;
    }

    if (cmd->output_file) {
        int flags = O_WRONLY | O_CREAT;
        if (cmd->append_output) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }

        redirect_out = open(cmd->output_file, flags);
        if (redirect_out < 0) {
            printf("Cannot open output file '%s'\n", cmd->output_file);
            if (redirect_in >= 0) {
                close(redirect_in);
                stdin = saved_stdin;
            }
            return 0;
        }
        stdout = redirect_out;
    }

    const char* command = cmd->args[0];

    // Built-in commands (only those that must be built-in)
    if (strcmp(command, "help") == 0) {
        cmd_help(cmd);
    } else if (strcmp(command, "hello") == 0) {
        cmd_hello(cmd);
    } else if (strcmp(command, "echo") == 0) {
        cmd_echo(cmd);
    } else if (strcmp(command, "cd") == 0) {
        cmd_cd(cmd);
    } else if (strcmp(command, "history") == 0) {
        cmd_history(cmd);
    } else if (strcmp(command, "clear") == 0) {
        cmd_clear(cmd);
    } else if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
        return -1;  // Signal to exit
    } else {
        // External command
        const char* prog = find_executable(command);
        if (prog) {
            int64_t rid = 0;
            char** argv = cmd->args;
            rid = spawn_realm(prog, argv, environ, NULL);
            if (rid < 0) {
                if (rid == -ENOEXEC) {
                    printf("%s: Not a valid executable\n", prog);
                } else {
                    printf("spawn failed: %s\n", strerror(rid));
                }
            } else {
                int status = 0;
                wait_realm(rid, &status);
                if (status != 0) {
                    printf("realm exited with status %d\n", status);
                }
            }
        } else {
            printf("Unknown command: %s\n", command);
            printf("Type 'help' for available commands.\n");
        }
    }

    if (redirect_in >= 0) {
        close(redirect_in);
        stdin = saved_stdin;
    }

    if (redirect_out >= 0) {
        close(redirect_out);
        stdout = saved_stdout;
    }

    return 0;
}

void build_prompt(char* prompt_buf, size_t buf_size) {
    const char* dir = current_dir;
    const char* last_slash = NULL;

    while (*dir) {
        if (*dir == '/') last_slash = dir;
        dir++;
    }

    char name[64];
    const char* src = NULL;

    if (last_slash && *(last_slash + 1))
        src = last_slash + 1;
    else
        src = "/";

    size_t i = 0;
    while (src[i] && i < sizeof(name) - 1) {
        name[i] = src[i];
        i++;
    }
    name[i] = '\0';

    snprintf(
        prompt_buf,
        buf_size,
        "\033[38;2;100;149;237m\033[48;2;30;30;46m  VesperaOS "
        "\033[38;2;30;30;46m\033[48;2;66;117;245m\ue0b0"
        "\033[38;2;255;255;255m\033[48;2;66;117;245m \uf07c %s "
        "\033[38;2;66;117;245m\033[49m\ue0b0"
        "\033[0m ",
        name
    );
}

void shell_main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    char buf[MAX_INPUT] = {0};
    command_t cmd;
    cmd_clear(NULL);

    printf(
        "\033[38;2;66;117;245m\n"
        " ==    ==                                                ====     ======  \n"
        " ==    ==                                               ==  ==   ==    == \n"
        " ==    ==                                              ==    ==  ==    == \n"
        " ==    ==   ===    ===   ====    ===   == ===    ===   ==    ==   ==      \n"
        " ===  ===  == ==  == ==  == ==  == ==  ==== ==  == ==  ==    ==     ==    \n"
        "  ==  ==   =====   ==    == ==  =====  ==          ==  ==    ==       ==  \n"
        "  ==  ==   ==       ==   ====   ==     ==        ====  ==    ==  ==    == \n"
        "   ====    == ==  == ==  ==     == ==  ==       == ==   ==  ==   ==    == \n"
        "    ==      ===    ===   ==      ===   ==        ====    ====     ======  \n\n\033[0m"
    );
    printf(
        "    running on x86_64  \ue0b1 \033[38;2;180;180;180m type \033[38;2;66;245;81mhelp\033[38;2;180;180;180m for "
        "commands\033[0m\n\n"
    );

    HANDLE hdl = open("/mnt/dev0/hello.txt", O_WRONLY);
    if (hdl < 0) {
        printf("Cannot open: %s", strerror(hdl));
    }
    ssize_t n = write(hdl, "Hello from write io", 20);
    if (hdl < 0) {
        printf("Cannot write: %s", strerror(n));
    }
    while (1) {
        char prompt_str[256];
        build_prompt(prompt_str, sizeof(prompt_str));
        int n = readline(prompt_str, buf, MAX_INPUT);
        if (n <= 0) continue;

        buf[n] = '\0';

        // Strip trailing newline(s)
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
            buf[n - 1] = '\0';
            n--;
        }

        // Skip empty lines
        char* trimmed = trim_whitespace(buf);
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

int main(int argc, char** argv) {
    setenv("SHELL", "/bin/nox", true);
    shell_main(argc, argv);
    return 0;
}