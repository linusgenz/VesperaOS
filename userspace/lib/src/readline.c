// readline.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 18.03.26.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#define RL_MAX 256
#define RL_HIST 32

static char rl_hist[RL_HIST][RL_MAX];
static int rl_hist_count = 0;
static int rl_hist_idx = 0;

static void rl_write(const char* s, size_t n) {
    sys_write(stdout, (uint64_t)s, n, 0, 0, 0);
}

static void rl_putc(char c) {
    rl_write(&c, 1);
}

// Move the terminal cursor left/right by delta columns using CSI sequences
static void rl_move_cursor(int delta) {
    if (delta == 0) return;
    char buf[16];
    int n = 0;
    if (delta > 0)
        n = snprintf(buf, sizeof(buf), "\033[%dC", delta);  // right
    else
        n = snprintf(buf, sizeof(buf), "\033[%dD", -delta);  // left
    rl_write(buf, n);
}

static void rl_redraw_tail(const char* buf, int len, int cur) {
    // Print chars from cur to end
    rl_write(buf + cur, len - cur);
    // Erase everything after the new end (handles deletion case)
    rl_write("\033[K", 3);
    rl_move_cursor(-(len - cur));
}

static void rl_history_push(const char* line) {
    if (!line || !*line) return;
    if (rl_hist_count > 0 && strcmp(rl_hist[(rl_hist_count - 1) % RL_HIST], line) == 0) return;
    strncpy(rl_hist[rl_hist_count % RL_HIST], line, RL_MAX - 1);
    rl_hist_count++;
    rl_hist_idx = rl_hist_count;
}

int readline(const char* prompt, char* out, size_t out_size) {
    if (!out || out_size == 0) return -1;

    if (prompt) rl_write(prompt, strlen(prompt));

    // Switch stdin to raw, no-echo mode
    tty_mode_t saved, raw;
    tcgetattr(stdin, &saved);
    raw.mode = TTY_MODE_RAW;
    raw.echo = 0;
    tcsetattr(stdin, &raw);

    char buf[RL_MAX];
    int len = 0;  // number of chars in buf
    int cur = 0;  // cursor position within buf
    int ret = 0;

    while (1) {
        char c = 0;
        if (sys_read(stdin, (uint64_t)&c, 1, 0, 0, 0) <= 0) break;

        // Escape sequence
        if (c == '\033') {
            char seq[4] = {0};
            sys_read(stdin, (uint64_t)&seq[0], 1, 0, 0, 0);  // '['
            sys_read(stdin, (uint64_t)&seq[1], 1, 0, 0, 0);  // letter or digit

            if (seq[0] == '[') {
                if (seq[1] == 'A' || (seq[1] == '5' && seq[2] == '~')) {
                    // Arrow-up, history previous
                    if (rl_hist_idx > 0 && rl_hist_count > 0) {
                        rl_hist_idx--;
                        const char* h = rl_hist[rl_hist_idx % RL_HIST];

                        rl_move_cursor(-cur);
                        rl_write("\033[K", 3);
                        len = strlen(h);
                        cur = len;
                        memcpy(buf, h, len);
                        rl_write(buf, len);
                    }
                } else if (seq[1] == 'B' || (seq[1] == '6' && seq[2] == '~')) {
                    // Arrow-down, history next
                    if (rl_hist_idx < rl_hist_count) {
                        rl_hist_idx++;
                        rl_move_cursor(-cur);
                        rl_write("\033[K", 3);
                        if (rl_hist_idx == rl_hist_count) {
                            len = 0;
                            cur = 0;
                        } else {
                            const char* h = rl_hist[rl_hist_idx % RL_HIST];
                            len = strlen(h);
                            cur = len;
                            memcpy(buf, h, len);
                            rl_write(buf, len);
                        }
                    }
                } else if (seq[1] == 'C') {
                    if (cur < len) {
                        cur++;
                        rl_move_cursor(1);
                    }
                } else if (seq[1] == 'D') {
                    if (cur > 0) {
                        cur--;
                        rl_move_cursor(-1);
                    }
                } else if (seq[1] == 'H') {
                    // Home
                    rl_move_cursor(-cur);
                    cur = 0;
                } else if (seq[1] == 'F') {
                    // End
                    rl_move_cursor(len - cur);
                    cur = len;
                } else if (seq[1] == '3') {
                    // Delete key: ESC[3~
                    sys_read(stdin, (uint64_t)&seq[2], 1, 0, 0, 0);  // '~'
                    if (cur < len) {
                        memmove(buf + cur, buf + cur + 1, len - cur - 1);
                        len--;
                        rl_redraw_tail(buf, len, cur);
                    }
                } else if (seq[1] == '5') {
                    // Page-up, history previous
                    sys_read(stdin, (uint64_t)&seq[2], 1, 0, 0, 0);  // '~'
                    if (rl_hist_idx > 0 && rl_hist_count > 0) {
                        rl_hist_idx--;
                        const char* h = rl_hist[rl_hist_idx % RL_HIST];

                        rl_move_cursor(-cur);
                        rl_write("\033[K", 3);
                        len = strlen(h);
                        cur = len;
                        memcpy(buf, h, len);
                        rl_write(buf, len);
                    }
                } else if (seq[1] == '6') {
                    // Page-down, history next
                    sys_read(stdin, (uint64_t)&seq[2], 1, 0, 0, 0);  // '~'
                    if (rl_hist_idx < rl_hist_count) {
                        rl_hist_idx++;
                        rl_move_cursor(-cur);
                        rl_write("\033[K", 3);
                        if (rl_hist_idx == rl_hist_count) {
                            len = 0;
                            cur = 0;
                        } else {
                            const char* h = rl_hist[rl_hist_idx % RL_HIST];
                            len = strlen(h);
                            cur = len;
                            memcpy(buf, h, len);
                            rl_write(buf, len);
                        }
                    }
                }
            }
            continue;
        }

        // Ctrl+C, cancel
        if (c == 3) {
            rl_putc('\n');
            len = 0;
            ret = 0;
            break;
        }

        // Enter
        if (c == '\n' || c == '\r') {
            rl_putc('\n');
            buf[len] = '\0';
            rl_history_push(buf);
            rl_hist_idx = rl_hist_count;
            ret = len;
            break;
        }

        // Backspace (0x7F or 0x08)
        if (c == 127 || c == '\b') {
            if (cur > 0) {
                memmove(buf + cur - 1, buf + cur, len - cur);
                cur--;
                len--;
                rl_move_cursor(-1);
                rl_redraw_tail(buf, len, cur);
            }
            continue;
        }

        // Ctrl+A / Ctrl+E
        if (c == 1) {
            rl_move_cursor(-cur);
            cur = 0;
            continue;
        }  // Ctrl+A
        if (c == 5) {
            rl_move_cursor(len - cur);
            cur = len;
            continue;
        }  // Ctrl+E

        // Ctrl+K — kill to end of line
        if (c == 11) {
            len = cur;
            rl_write("\033[K", 3);
            continue;
        }

        // Printable character
        if ((unsigned char)c >= 0x20 && len < (int)out_size - 1) {
            // Insert at cursor
            if (cur < len) {
                memmove(buf + cur + 1, buf + cur, len - cur);
            }
            buf[cur] = c;
            len++;
            cur++;
            rl_write(buf + cur - 1, len - cur + 1);
            if (cur < len) {
                rl_move_cursor(-(len - cur));
            }
        }
    }

    // Restore canonical mode
    tcsetattr(stdin, &saved);

    if (ret > 0) {
        const size_t copy = (size_t)ret < out_size - 1 ? (size_t)ret : out_size - 1;
        memcpy(out, buf, copy);
        out[copy] = '\0';
        return (int)copy;
    }
    out[0] = '\0';
    return 0;
}