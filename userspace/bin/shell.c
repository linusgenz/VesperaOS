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

#define BUFSIZ 8192

#include <dirent.h>
#include <errno.h>
#include <exec.h>
#include <fflags.h>
#include <readline.h>
#include <realm.h>
#include <reboot.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysstd.h>

#include "vespera/stat.h"

typedef struct {
    char* args[MAX_ARGS];
    int argc;
    char* input_file;
    char* output_file;
    bool append_output;
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

int parse_command(char* input, command_t* cmd) {
    if (!input || !cmd) return -1;

    cmd->argc = 0;
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->append_output = false;

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

        cmd->args[cmd->argc++] = token;

        // Find end of token
        while (*token && *token != ' ' && *token != '\t') token++;
        if (*token) *token++ = '\0';
    }

    cmd->args[cmd->argc] = NULL;
    return cmd->argc;
}

void cmd_help(command_t* cmd) {
    printf("\033[38;2;66;117;245m\uf489  VesperaOS Shell v2.0\033[0m\n\n");
    printf("\033[38;2;180;180;180mAvailable commands:\033[0m\n\n");

    printf("  \033[38;2;66;245;81m\uf002\033[0m  help      - Show this help\n");
    printf("  \033[38;2;66;245;81m\uf07b\033[0m  ls        - List directory\n");
    printf("  \033[38;2;66;245;81m\uf015\033[0m  cd        - Change directory\n");
    printf("  \033[38;2;66;245;81m\uf46d\033[0m  cat       - Show file contents\n");
    printf("  \033[38;2;66;245;81m\uf0ac\033[0m  pwd       - Print working directory\n");
    printf("  \033[38;2;66;245;81m\uf1da\033[0m  history   - Command history\n");
    printf("  \033[38;2;66;245;81m\uf12d\033[0m  clear     - Clear screen\n");
    printf("  \033[38;2;245;100;80m\uf011\033[0m  shutdown  - Shutdown the PC\n");
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

void cmd_echo(command_t* cmd) {
    for (int i = 1; i < cmd->argc; i++) {
        if (i > 1) printf(" ");
        printf(cmd->args[i]);
    }
    printf("\n");
}

void cmd_pwd(command_t* cmd) {
    char cwd[MAX_PATH];
    if (getcwd(cwd, sizeof(cwd)) >= 0) {
        printf("%s", cwd);
        printf("\n");
    } else {
        printf(current_dir);
        printf("\n");
    }
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
    int start = (history_count > HISTORY_SIZE) ? history_count - HISTORY_SIZE : 0;
    int end = history_count;

    for (int i = start; i < end; i++) {
        printf("%d: %s\n", i + 1, history[i % HISTORY_SIZE]);
    }
}

void cmd_clear(command_t* cmd) {
    printf("\033[2J\033[H");  // ANSI escape codes
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

void cmd_ls(command_t* cmd) {
    int long_fmt = 0;
    int classify = 0;
    int show_all = 0;
    const char* path = NULL;

    for (int i = 1; i < cmd->argc; i++) {
        if (cmd->args[i][0] == '-') {
            for (int j = 1; cmd->args[i][j]; j++) {
                switch (cmd->args[i][j]) {
                    case 'l':
                        long_fmt = 1;
                        break;
                    case 'F':
                        classify = 1;
                        break;
                    case 'a':
                        show_all = 1;
                        break;
                    default:;
                }
            }
        } else {
            path = cmd->args[i];
        }
    }

    if (!path) path = current_dir;

    DIR_HANDLE hdl = opendir(path);
    if (hdl < 0) {
        if (hdl == -ENOENT)
            printf("ls: cannot open '%s': No such file or directory\n", path);
        else
            printf("ls: cannot open '%s' (error %ld)\n", path, hdl);
        return;
    }

    dirent_t ent;
    while (readdir(hdl, &ent) > 0) {
        // . und .. ausblenden wenn kein -a
        if (!show_all) {
            if (ent.name[0] == '.' && (ent.name[1] == '\0' || (ent.name[1] == '.' && ent.name[2] == '\0'))) continue;
        }

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
                color = "\033[0m";
                break;
        }

        if (!long_fmt) {
            printf("%s%s%s\033[0m ", color, ent.name, indicator);
            continue;
        }

        // -l: stat für Größe und Flags
        char full_path[512];
        if (strcmp(path, "/") == 0)
            snprintf(full_path, sizeof(full_path), "/%s", ent.name);
        else
            snprintf(full_path, sizeof(full_path), "%s/%s", path, ent.name);

        vespera_stat_t st;
        int has_stat = (sys_stat((uint64_t)full_path, (uint64_t)&st, 0, 0, 0, 0) == 0);

        char type_char = 0;
        switch (ent.type) {
            case DT_DIR:
                type_char = 'd';
                break;
            case DT_CHARDEV:
                type_char = 'c';
                break;
            case DT_BLOCKDEV:
                type_char = 'b';
                break;
            case DT_SYMLINK:
                type_char = 'l';
                break;
            default:
                type_char = '-';
                break;
        }

        char rw[4];
        if (has_stat) {
            rw[0] = (st.flags & VSTAT_FLAG_READABLE) ? 'r' : '-';
            rw[1] = (st.flags & VSTAT_FLAG_WRITABLE) ? 'w' : '-';
            rw[2] = (st.flags & VSTAT_FLAG_EXEC) ? 'x' : '-';
            rw[3] = '\0';
        } else {
            rw[0] = 'r';
            rw[1] = '-';
            rw[2] = '-';
            rw[3] = '\0';
        }

        char size_buf[16] = "     -";
        if (has_stat && ent.type != DT_DIR) format_size(st.size, size_buf, sizeof(size_buf));

        printf("%c%s  %s  %s%s%s\033[0m\n", type_char, rw, size_buf, color, ent.name, indicator);
    }

    if (!long_fmt) putchar('\n');
    closedir(hdl);
}

void cmd_cat(command_t* cmd) {
    if (cmd->argc < 2) {
        printf("Usage: cat <file>\n");
        return;
    }

    const char* path = cmd->args[1];
    FILE_HANDLE fd = open(path, O_RDONLY);

    if (fd < 0) {
        if (fd == -ENOENT) {
            printf("cat: %s: No such file or directory\n", path);
        } else if (fd == -EISDIR) {
            printf("cat: %s: Is a directory\n", path);
        } else {
            printf("cat: %s: Cannot open file (error %ld)\n", path, fd);
        }
        return;
    }

    char buffer[BUFSIZ];
    ssize_t bytes_read = 0;

    printf("reading from '%s'\n", path);
    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }

    if (bytes_read < 0) {
        printf("\ncat: Error reading file\n");
    }

    close(fd);
}

void cmd_mkdir(command_t* cmd) {
    if (cmd->argc < 2) {
        printf("Usage: mkdir <directory>\n");
        return;
    }

    const char* path = cmd->args[1];
    int64_t res = create(path, C_DIR);

    if (res < 0) {
        switch ((int)res) {
            case -EEXIST:
                printf("mkdir: cannot create directory '%s': File exists\n", path);
                break;
            case -EACCES:
                printf("mkdir: cannot create directory '%s': Permission denied\n", path);
                break;
            case -EROFS:
                printf("mkdir: cannot create directory '%s': Read-only filesystem\n", path);
                break;
            case -ENOSPC:
                printf("mkdir: cannot create directory '%s': No space left on device\n", path);
                break;
            default:
                printf("mkdir: cannot create directory '%s' (error %ld)\n", path, res);
                break;
        }
        return;
    }

    printf("Directory '%s' created successfully.\n", path);
}

void cmd_rmdir(command_t* cmd) {
    if (cmd->argc < 2) {
        printf("Usage: rmdir <directory>\n");
        return;
    }

    const char* path = cmd->args[1];
    int64_t res = rmdir(path);

    if (res < 0) {
        switch ((int)res) {
            case -ENOENT:
                printf("rmdir: failed to remove '%s': No such file or directory\n", path);
                break;
            case -ENOTDIR:
                printf("rmdir: failed to remove '%s': Not a directory\n", path);
                break;
            case -ENOTEMPTY:
                printf("rmdir: failed to remove '%s': Directory not empty\n", path);
                break;
            case -EACCES:
                printf("rmdir: failed to remove '%s': Permission denied\n", path);
                break;
            default:
                printf("rmdir: failed to remove '%s' (error %ld)\n", path, res);
                break;
        }
        return;
    }

    printf("Directory '%s' removed successfully.\n", path);
}

void cmd_touch(command_t* cmd) {
    if (cmd->argc < 2) {
        printf("Usage: touch <file>\n");
        return;
    }

    const char* path = cmd->args[1];
    FILE_HANDLE fd = open(path, O_CREAT | O_RDWR);

    if (fd < 0) {
        switch ((int)fd) {
            case -EACCES:
                printf("touch: cannot create file '%s': Permission denied\n", path);
                break;
            case -EROFS:
                printf("touch: cannot create file '%s': Read-only filesystem\n", path);
                break;
            default:
                printf("touch: cannot create file '%s' (error %ld)\n", path, fd);
                break;
        }
        return;
    }

    // Datei existiert -> eventuell Zeitstempel aktualisieren (optional)
    close(fd);
    printf("File '%s' created or updated successfully.\n", path);
}

void cmd_cp(command_t* cmd) {
    if (cmd->argc < 3) {
        printf("Usage: cp <source> <dest>\n");
        return;
    }

    FILE_HANDLE src = open(cmd->args[1], O_RDONLY);
    if (src < 0) {
        printf("cp: cannot open '%s'\n", cmd->args[1]);
        return;
    }

    FILE_HANDLE dst = open(cmd->args[2], O_WRONLY | O_CREAT | O_TRUNC);
    if (dst < 0) {
        printf("cp: cannot create '%s'\n", cmd->args[2]);
        close(src);
        return;
    }

    char buffer[4096];
    ssize_t bytes = 0;
    while ((bytes = read(src, buffer, sizeof(buffer))) > 0) {
        if (write(dst, buffer, bytes) != bytes) {
            printf("cp: write error\n");
            break;
        }
    }

    close(src);
    close(dst);
}

void cmd_rm(command_t* cmd) {
    if (cmd->argc < 2) {
        printf("Usage: rm <file>\n");
        return;
    }

    if (unlink(cmd->args[1]) == 0) {
        printf("Removed '%s'\n", cmd->args[1]);
    } else {
        printf("rm: cannot remove '%s'\n", cmd->args[1]);
    }
}

int execute_command(command_t* cmd) {
    if (cmd->argc == 0) return 0;

    const char* command = cmd->args[0];

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

    if (strcmp(command, "help") == 0) {
        cmd_help(cmd);
    } else if (strcmp(command, "ls") == 0) {
        cmd_ls(cmd);
    } else if (strcmp(command, "cp") == 0) {
        cmd_cp(cmd);
    } else if (strcmp(command, "rm") == 0) {
        cmd_rm(cmd);
    } else if (strcmp(command, "hello") == 0) {
        cmd_hello(cmd);
    } else if (strcmp(command, "echo") == 0) {
        cmd_echo(cmd);
    } else if (strcmp(command, "cat") == 0) {
        cmd_cat(cmd);
    } else if (strcmp(command, "pwd") == 0) {
        cmd_pwd(cmd);
    } else if (strcmp(command, "cd") == 0) {
        cmd_cd(cmd);
    } else if (strcmp(command, "history") == 0) {
        cmd_history(cmd);
    } else if (strcmp(command, "clear") == 0) {
        cmd_clear(cmd);
    } else if (strcmp(command, "mkdir") == 0) {
        cmd_mkdir(cmd);
    } else if (strcmp(command, "rmdir") == 0) {
        cmd_rmdir(cmd);
    } else if (strcmp(command, "touch") == 0) {
        cmd_touch(cmd);
    } else if (strcmp(command, "shutdown") == 0) {
        reboot_poweroff();
    } else if (strcmp(command, "reboot") == 0) {
        puts("Rebooting...\n");
        reboot_restart();
    } else if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
        return -1;  // Signal to exit
    } else {
        const char* prog = find_executable(command);
        if (prog) {
            int64_t rid = 0;
            char** argv = cmd->args;
            rid = spawn_realm(prog, argv, NULL);
            if (rid < 0) {
                printf("spawn failed: %d\n", (int32_t)rid);
            } else {
                int status = 0;
                wait_realm(rid, &status);
                if (status != 0) {
                    printf("realm exited with status %d", status);
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

    char name[64];  // ausreichend für Prompt
    const char* src = NULL;

    if (last_slash && *(last_slash + 1))
        src = last_slash + 1;
    else
        src = "/";

    // Kopieren mit Längenbegrenzung
    size_t i = 0;
    while (src[i] && i < sizeof(name) - 1) {
        name[i] = src[i];
        i++;
    }
    name[i] = '\0';

    snprintf(
        prompt_buf,
        buf_size,
        "\033[38;2;100;149;237m\033[48;2;30;30;46m  VesperaOS "   // blauer Block
        "\033[38;2;30;30;46m\033[48;2;66;117;245m\ue0b0"          // Pfeil-Transition
        "\033[38;2;255;255;255m\033[48;2;66;117;245m \uf07c %s "  // Ordner + Name
        "\033[38;2;66;117;245m\033[49m\ue0b0"                     // zweiter Pfeil
        "\033[0m ",
        name
    );
}

void shell_main(int argc, char** argv) {
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
        "    running on x86_64  \ue0b1 \033[38;2;180;180;180m type \033[38;2;66;245;81mhelp\033[38;2;180;180;180m for commands\033[0m\n\n"
    );

    /* image_t img;
     int result = jpeg_load_from_file("test.jpg", &img, NULL);

     if (result != JPEG_OK)
     {
         printf("Error loading JPEG: %s\n", jpeg_error_string(result));
         return;
     }
     printf("environ: %p", environ);

     printf("Loaded image: %lu x %lu, %d channels\n",
            img.width, img.height, img.channels);

     fb_blit bltcmd = {
         .buffer_height = img.height,
         .buffer_width = img.width,
         .dst_x = 0,
         .dst_y = 0,
         .pixels = img.data
     };
     HANDLE hdl = open("/dev/fb0", O_RDWR);
     ioctl(hdl, FB_IOCTL_BLIT, &bltcmd);

     image_free(&img);*/

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
    shell_main(argc, argv);
    return 0;
}
