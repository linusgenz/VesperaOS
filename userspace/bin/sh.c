// sh.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 28.04.26.
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
#include <exec.h>
#include <realm.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <fflags.h>
#include <vespera/handles.h>
// ── limits ──────────────────────────────────────────────────────────────────

#define SH_LINE_MAX  1024   // max characters per input line
#define SH_ARGS_MAX  64     // max words per command
#define SH_PIPE_MAX  8      // max commands in a pipeline

// ── internal last-exit-status ───────────────────────────────────────────────

static int g_last_status = 0;     // value of $?
static bool g_interactive = false;
static bool g_exit_on_error = false; // -e flag

// ── word / token ─────────────────────────────────────────────────────────────

// A single parsed word after expansion.
typedef struct {
    char buf[SH_LINE_MAX];  // expanded text
} word_t;

// ── command ──────────────────────────────────────────────────────────────────

typedef struct cmd {
    char*  argv[SH_ARGS_MAX + 1];   // NULL-terminated argument vector
    int    argc;
    char*  redir_in;                // < file  (or NULL)
    char*  redir_out;               // > file  (or NULL)
    bool   redir_out_append;        // >> instead of >
    bool   pipe_out;                // stdout goes to next cmd's stdin
} cmd_t;

typedef struct pipeline {
    cmd_t  cmds[SH_PIPE_MAX];
    int    ncmds;
} pipeline_t;

// ── tiny string helpers ──────────────────────────────────────────────────────

static char* sh_trim(char* s) {
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    char* e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r'))
        *--e = '\0';
    return s;
}

// ── variable expansion ───────────────────────────────────────────────────────

// Expand $VAR / ${VAR} / $? inside src into dst (dst_size bytes).
// Returns dst.
static char* sh_expand(const char* src, char* dst, size_t dst_size) {
    size_t di = 0;
    const char* s = src;

    while (*s && di + 1 < dst_size) {
        if (*s == '$') {
            s++;
            // $? → last exit status
            if (*s == '?') {
                char num[16];
                snprintf(num, sizeof(num), "%d", g_last_status);
                for (const char* p = num; *p && di + 1 < dst_size; p++)
                    dst[di++] = *p;
                s++;
                continue;
            }
            // ${VAR} or $VAR
            char name[128];
            size_t ni = 0;
            bool braced = (*s == '{');
            if (braced) s++;
            while (*s && ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
                           (*s >= '0' && *s <= '9') || *s == '_') && ni + 1 < sizeof(name))
                name[ni++] = *s++;
            if (braced && *s == '}') s++;
            name[ni] = '\0';
            if (ni > 0) {
                const char* val = getenv(name);
                if (val) {
                    for (; *val && di + 1 < dst_size; val++)
                        dst[di++] = *val;
                }
            } else {
                // bare '$' with no name
                dst[di++] = '$';
            }
            continue;
        }
        dst[di++] = *s++;
    }
    dst[di] = '\0';
    return dst;
}

// ── tokenizer ────────────────────────────────────────────────────────────────

// Split line into words stored in argv[].  Handles single/double quotes and
// $-expansion. Returns word count, -1 on error.
static int sh_tokenize(char* line, char** argv, int argv_max, word_t* words) {
    int argc = 0;
    char* p = line;

    while (*p && argc < argv_max) {
        // skip whitespace
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n') break;

        // comment
        if (*p == '#') break;

        char raw[SH_LINE_MAX];
        size_t ri = 0;

        while (*p && *p != ' ' && *p != '\t' && *p != '\n') {
            if (*p == '\'') {
                // single-quoted: no expansion
                p++;
                while (*p && *p != '\'') {
                    if (ri + 1 < sizeof(raw)) raw[ri++] = *p;
                    p++;
                }
                if (*p == '\'') p++;
            } else if (*p == '"') {
                // double-quoted: $-expansion but no word splitting
                p++;
                while (*p && *p != '"') {
                    if (*p == '$') {
                        // collect the $... token and expand it
                        char tmp[SH_LINE_MAX];
                        size_t ti = 0;
                        tmp[ti++] = *p++;
                        while (*p && *p != '"' && *p != ' ' && ti + 1 < sizeof(tmp))
                            tmp[ti++] = *p++;
                        tmp[ti] = '\0';
                        char expanded[SH_LINE_MAX];
                        sh_expand(tmp, expanded, sizeof(expanded));
                        for (const char* e = expanded; *e && ri + 1 < sizeof(raw); e++)
                            raw[ri++] = *e;
                    } else {
                        if (ri + 1 < sizeof(raw)) raw[ri++] = *p++;
                    }
                }
                if (*p == '"') p++;
            } else if (*p == '>' || *p == '<' || *p == '|') {
                // redirection / pipe operators stop the current word
                break;
            } else {
                if (ri + 1 < sizeof(raw)) raw[ri++] = *p++;
            }
        }

        if (ri == 0 && (*p == '>' || *p == '<' || *p == '|')) {
            // operator token
            raw[ri++] = *p++;
            if (*p == '>' && raw[0] == '>') { raw[ri++] = *p++; } // >>
        }

        raw[ri] = '\0';

        // expand $vars in unquoted token
        sh_expand(raw, words[argc].buf, SH_LINE_MAX);
        argv[argc] = words[argc].buf;
        argc++;
    }

    argv[argc] = NULL;
    return argc;
}

// ── pipeline parser ──────────────────────────────────────────────────────────

// Parse a line into a pipeline_t.  Returns 0 on success, -1 on syntax error.
static int sh_parse(char* line, pipeline_t* pl, word_t* words) {
    memset(pl, 0, sizeof(*pl));

    // We'll gather all tokens first, then split on '|'.
    char* argv[SH_ARGS_MAX + 1];
    int argc = sh_tokenize(line, argv, SH_ARGS_MAX, words);
    if (argc <= 0) return argc;

    int ci = 0;   // current command index
    pl->ncmds = 1;
    cmd_t* cmd = &pl->cmds[0];
    memset(cmd, 0, sizeof(*cmd));

    for (int i = 0; i < argc; i++) {
        const char* tok = argv[i];

        if (strcmp(tok, "|") == 0) {
            cmd->argv[cmd->argc] = NULL;
            cmd->pipe_out = true;
            if (++ci >= SH_PIPE_MAX) {
                fprintf(stderr, "sh: pipeline too long\n");
                return -1;
            }
            pl->ncmds++;
            cmd = &pl->cmds[ci];
            memset(cmd, 0, sizeof(*cmd));

        } else if (strcmp(tok, "<") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "sh: expected filename after <\n"); return -1; }
            cmd->redir_in = argv[++i];

        } else if (strcmp(tok, ">>") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "sh: expected filename after >>\n"); return -1; }
            cmd->redir_out = argv[++i];
            cmd->redir_out_append = true;

        } else if (strcmp(tok, ">") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "sh: expected filename after >\n"); return -1; }
            cmd->redir_out = argv[++i];
            cmd->redir_out_append = false;

        } else {
            if (cmd->argc >= SH_ARGS_MAX) {
                fprintf(stderr, "sh: too many arguments\n");
                return -1;
            }
            cmd->argv[cmd->argc++] = argv[i];
        }
    }

    cmd->argv[cmd->argc] = NULL;
    return 0;
}

// ── built-in commands ────────────────────────────────────────────────────────

// Returns -1 if not a builtin, otherwise the exit code of the builtin.
static int sh_builtin(cmd_t* cmd) {
    if (cmd->argc == 0) return -1;
    const char* name = cmd->argv[0];

    // ── : / true ────────────────────────────────────────────────────────────
    if (strcmp(name, ":") == 0 || strcmp(name, "true") == 0)
        return 0;

    // ── false ────────────────────────────────────────────────────────────────
    if (strcmp(name, "false") == 0)
        return 1;

    // ── exit ─────────────────────────────────────────────────────────────────
    if (strcmp(name, "exit") == 0) {
        int code = (cmd->argc > 1) ? atoi(cmd->argv[1]) : g_last_status;
        exit((uint64_t)code);
    }

    // ── cd ───────────────────────────────────────────────────────────────────
    if (strcmp(name, "cd") == 0) {
        const char* path = (cmd->argc > 1) ? cmd->argv[1] : "/";
        if (chdir(path) != 0) {
            fprintf(stderr, "sh: cd: %s: no such directory\n", path);
            return 1;
        }
        // update $PWD
        char cwd[SH_LINE_MAX];
        if (getcwd(cwd, sizeof(cwd)) == 0)
            setenv("PWD", cwd, 1);
        return 0;
    }

    // ── pwd ──────────────────────────────────────────────────────────────────
    if (strcmp(name, "pwd") == 0) {
        char cwd[SH_LINE_MAX];
        if (getcwd(cwd, sizeof(cwd)) == 0) {
            printf("%s\n", cwd);
            return 0;
        }
        fprintf(stderr, "sh: pwd: failed\n");
        return 1;
    }

    // ── echo ─────────────────────────────────────────────────────────────────
    if (strcmp(name, "echo") == 0) {
        bool newline = true;
        int start = 1;
        if (cmd->argc > 1 && strcmp(cmd->argv[1], "-n") == 0) {
            newline = false;
            start = 2;
        }
        for (int i = start; i < cmd->argc; i++) {
            if (i > start) putchar(' ');
            printf("%s", cmd->argv[i]);
        }
        if (newline) putchar('\n');
        return 0;
    }

    // ── export ───────────────────────────────────────────────────────────────
    if (strcmp(name, "export") == 0) {
        for (int i = 1; i < cmd->argc; i++) {
            char* eq = strchr(cmd->argv[i], '=');
            if (eq) {
                *eq = '\0';
                setenv(cmd->argv[i], eq + 1, 1);
                *eq = '=';
            }
            // export NAME alone: already in environment, nothing to do
        }
        return 0;
    }

    // ── unset ────────────────────────────────────────────────────────────────
    if (strcmp(name, "unset") == 0) {
        for (int i = 1; i < cmd->argc; i++)
            unsetenv(cmd->argv[i]);
        return 0;
    }

    return -1;  // not a builtin
}

// ── single-command executor ──────────────────────────────────────────────────

// Execute one cmd_t.  stdin/stdout are already wired by the caller when this
// is part of a pipeline.  Returns exit status (0 = success).
static int sh_exec_cmd(cmd_t* cmd) {
    if (cmd->argc == 0) return 0;

    // Built-ins
    int bret = sh_builtin(cmd);
    if (bret >= 0) return bret;

    // External command
    const char* prog = find_executable(cmd->argv[0]);
    if (!prog) {
        fprintf(stderr, "sh: %s: command not found\n", cmd->argv[0]);
        return 127;
    }

    int64_t rid = spawn_realm(prog, cmd->argv, environ, NULL);
    if (rid < 0) {
        if (rid == -ENOEXEC)
            fprintf(stderr, "sh: %s: not executable\n", prog);
        else
            fprintf(stderr, "sh: spawn failed: %s\n", strerror((int)-rid));
        return 126;
    }

    sys_tcsetpgrp(HANDLE_STDOUT, (uint64_t)rid, 0, 0, 0, 0);

    int status = 0;
    wait_realm(rid, &status);

    int64_t my_id = get_realm_id();
    sys_tcsetpgrp(HANDLE_STDOUT, (uint64_t)my_id, 0, 0, 0, 0);

    if (WIFSIGNALED(status)) {
        if (WTERMSIG(status) != 2)  // SIGINT is silent
            fprintf(stderr, "sh: killed by signal %d\n", WTERMSIG(status));
        return 128 + WTERMSIG(status);
    }
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return 0;
}

// ── pipeline executor ─────────────────────────────────────────────────────────

// Execute a single-command pipeline (no actual pipes yet — pipe support
// requires kernel-side pipe handles which are stubbed out in nox too).
// I/O redirection is applied here.
static int sh_exec_pipeline(pipeline_t* pl) {
    if (pl->ncmds == 0) return 0;

    // For now execute commands sequentially without inter-process pipes.
    // (sys_pipe is declared but its implementation in nox is also commented
    //  out / stubbed, so we mirror that behaviour here.)
    int last = 0;

    for (int i = 0; i < pl->ncmds; i++) {
        cmd_t* cmd = &pl->cmds[i];

        FILE* saved_in  = stdin;
        FILE* saved_out = stdout;
        FILE* fin  = NULL;
        FILE* fout = NULL;

        // Input redirect
        if (cmd->redir_in) {
            fin = fopen(cmd->redir_in, "r");
            if (!fin) {
                fprintf(stderr, "sh: %s: cannot open for reading\n", cmd->redir_in);
                last = 1;
                continue;
            }
            stdin = fin;
        }

        // Output redirect
        if (cmd->redir_out) {
            fout = fopen(cmd->redir_out, cmd->redir_out_append ? "a" : "w");
            if (!fout) {
                fprintf(stderr, "sh: %s: cannot open for writing\n", cmd->redir_out);
                if (fin) { fclose(fin); stdin = saved_in; }
                last = 1;
                continue;
            }
            stdout = fout;
        }

        last = sh_exec_cmd(cmd);

        if (fin)  { fclose(fin);  stdin  = saved_in;  }
        if (fout) { fclose(fout); stdout = saved_out; }
    }

    return last;
}

// ── line executor ─────────────────────────────────────────────────────────────

// Parse and run one logical line.  Returns exit status of the last command.
// A negative return means "exit the shell".
static int sh_run_line(char* line) {
    line = sh_trim(line);
    if (line[0] == '\0' || line[0] == '#') return 0;

    // Allocate word storage on the stack (each cmd word needs a buffer).
    word_t words[SH_ARGS_MAX];

    pipeline_t pl;
    if (sh_parse(line, &pl, words) < 0) return 1;
    if (pl.ncmds == 0 || pl.cmds[0].argc == 0) return 0;

    // Intercept shell-level "exit" before spawning anything
    if (strcmp(pl.cmds[0].argv[0], "exit") == 0) {
        int code = (pl.cmds[0].argc > 1) ? atoi(pl.cmds[0].argv[1]) : g_last_status;
        exit((uint64_t)code);
    }

    int status = sh_exec_pipeline(&pl);
    g_last_status = status;
    return status;
}

// ── script mode ──────────────────────────────────────────────────────────────

static int sh_run_script(const char* path, int argc, char** argv) {
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "sh: %s: cannot open\n", path);
        return 1;
    }

    // Set $0..$N for the script
    setenv("0", path, 1);
    for (int i = 0; i < argc; i++) {
        char name[8];
        snprintf(name, sizeof(name), "%d", i + 1);
        setenv(name, argv[i], 1);
    }

    char line[SH_LINE_MAX];
    int lineno = 0;
    int status = 0;

    while (fgets(line, sizeof(line), f)) {
        lineno++;
        // Strip trailing newline
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r'))
            line[--n] = '\0';

        // Skip shebang on first line
        if (lineno == 1 && line[0] == '#' && line[1] == '!') continue;

        status = sh_run_line(line);

        if (g_exit_on_error && status != 0) {
            fprintf(stderr, "sh: %s: line %d: command exited with status %d\n",
                    path, lineno, status);
            break;
        }
    }

    fclose(f);
    return status;
}

// ── interactive REPL ──────────────────────────────────────────────────────────

static void sh_repl(void) {
    char line[SH_LINE_MAX];

    while (1) {
        // Minimal prompt: "$ " for root-like, or just "$ "
        if (g_interactive) {
            const char* ps1 = getenv("PS1");
            if (!ps1) ps1 = "$ ";
            printf("%s", ps1);
            fflush(stdout);
        }

        if (!fgets(line, sizeof(line), stdin)) {
            // EOF (Ctrl+D)
            if (g_interactive) putchar('\n');
            break;
        }

        sh_run_line(line);
    }
}

// ── entry point ───────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    setenv("SHELL", "/bin/sh", 1);

    // Parse shell flags and operands
    int argi = 1;
    for (; argi < argc; argi++) {
        if (argv[argi][0] != '-') break;
        if (strcmp(argv[argi], "--") == 0) { argi++; break; }

        for (int fi = 1; argv[argi][fi]; fi++) {
            switch (argv[argi][fi]) {
                case 'e': g_exit_on_error = true; break;
                // -c "command string"
                case 'c':
                    if (argv[argi][fi+1] == '\0' && argi + 1 < argc) {
                        int st = sh_run_line(argv[++argi]);
                        return st;
                    }
                    break;
                default:
                    fprintf(stderr, "sh: unknown flag -%c\n", argv[argi][fi]);
                    break;
            }
        }
    }

    // Script mode: sh script.sh [args...]
    if (argi < argc) {
        int st = sh_run_script(argv[argi], argc - argi - 1, argv + argi + 1);
        return st;
    }

    // Interactive / stdin mode
    g_interactive = true; // treat as interactive when no script
    sh_repl();
    return g_last_status;
}

