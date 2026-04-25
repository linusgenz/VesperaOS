// vedit.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 19.03.26.
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
#include <fflags.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "vespera/handles.h"

static inline int tty_has_input(int timeout_ms) {
    pollhdl_t ph = {.hdl = HANDLE_STDIN, .events = POLLIN, .revents = 0, ._pad = 0};
    return (int)sys_poll((uintptr_t)&ph, 1, (uint32_t)timeout_ms, 0, 0, 0);
}

#define CSI "\033["
#define RESET "\033[0m"
#define HIDE_CUR "\033[?25l"
#define SHOW_CUR "\033[?25h"

/* Editor background palette */
#define BG_BASE "\033[48;2;28;28;40m"
#define BG_GUTTER "\033[48;2;36;36;50m"
#define BG_CURLINE "\033[48;2;42;42;62m"
#define BG_SB "\033[48;2;66;117;245m"    /* blue status bar */
#define BG_SB2 "\033[48;2;44;56;90m"     /* darker mid segment */
#define BG_SB_DARK "\033[48;2;30;30;46m" /* darkest segment */

/* Foreground */
#define FG_WHITE "\033[38;2;215;215;230m"
#define FG_DIM "\033[38;2;85;85;105m"
#define FG_LINENO "\033[38;2;80;80;105m"
#define FG_CURLN "\033[38;2;150;155;190m"
#define FG_SB_DARK "\033[38;2;28;28;40m"
#define FG_SB_MID "\033[38;2;44;56;90m"
#define FG_SB_BLUE "\033[38;2;66;117;245m"
#define FG_SB_LIGHT "\033[38;2;180;205;245m"

/* Syntax colours */
#define SYN_KW "\033[38;2;130;160;250m"   /* keyword */
#define SYN_TYPE "\033[38;2;180;120;240m" /* type */
#define SYN_STR "\033[38;2;100;205;100m"  /* string / char literal */
#define SYN_CMNT "\033[38;2;85;105;85m"   /* comment */
#define SYN_PREP "\033[38;2;65;200;200m"  /* preprocessor */
#define SYN_NUM "\033[38;2;230;155;55m"   /* number */
#define SYN_NORM FG_WHITE

/* Nerd Font glyphs (UTF-8 encoded) */
#define GL_FILE "\xef\x85\x9c"   /* U+F15C  nf-fa-file_text_o   */
#define GL_PENCIL "\xef\x81\x80" /* U+F040  nf-fa-pencil        */
#define GL_SAVED "\xef\x80\x8c"  /* U+F00C  nf-fa-check         */
#define GL_SAVE "\xef\x83\x87"   /* U+F0C7  nf-fa-floppy_o      */
#define GL_QUIT "\xef\x80\x91"   /* U+F011  nf-fa-power_off     */
#define GL_FIND "\xef\x80\x82"   /* U+F002  nf-fa-search        */
#define GL_CUT "\xef\x83\x84"    /* U+F0C4  nf-fa-scissors      */
#define GL_PASTE "\xef\x83\xaa"  /* U+F0EA  nf-fa-clipboard     */
#define GL_GOTO "\xef\x82\xa9"   /* U+F0A9  nf-fa-arrow_circle_right */
#define GL_SAVEAS "\xef\x83\x9b" /* U+F0DB  nf-fa-columns       */
#define GL_PL_R "\xee\x82\xb0"   /* U+E0B0  powerline solid-right  */
#define GL_PL_L "\xee\x82\xb2"   /* U+E0B2  powerline solid-left   */
#define GL_PL_RT "\xee\x82\xb1"  /* U+E0B1  powerline thin-right   */
#define GL_LINES "\xef\x83\x89"  /* U+F0C9  nf-fa-bars          */
#define GL_WARN "\xef\x81\xb1"   /* U+F071  nf-fa-warning       */
#define GL_BOX "\xe2\x94\x82"    /* U+2502  BOX DRAWINGS LIGHT VERTICAL │ */

#define KEY_NONE 0
#define KEY_UP 1000
#define KEY_DOWN 1001
#define KEY_LEFT 1002
#define KEY_RIGHT 1003
#define KEY_HOME 1004
#define KEY_END 1005
#define KEY_PAGE_UP 1006
#define KEY_PAGE_DOWN 1007
#define KEY_DEL 1008

#define CTRL(c) ((c) & 0x1f)

#define MAX_FILENAME 256
#define TAB_WIDTH 4
#define RBUF_CAP (256 * 1024)
#define LINE_INIT_CAP 64

typedef struct {
    char* data;
    int len;
    int cap;
} Line;

typedef struct {
    int  cx, cy;
    int  row_off, col_off;
    int  nlines;
    bool modified;
    bool botbar_rendered;
} RenderCache;

typedef struct {
    Line* lines;
    int nlines;
    int lines_cap;

    int cx, cy;
    int row_off, col_off;

    int rows, cols;
    int edit_rows; /* rows - 2 (top bar + bottom bar) */
    tty_mode_t orig_mode;

    char filename[MAX_FILENAME];
    bool modified;

    char* clip;
    int clip_len;

    bool quit_pending;

    char msg[160];
    bool msg_err;

    bool in_prompt;
    int prompt_col;

    char last_query[128];

    char* rbuf;
    int rlen;

    bool* line_dirty;
    RenderCache rc;
} Editor;

static Editor E;

static void rb_app(const char* s, int n) {
    if (E.rlen + n >= RBUF_CAP) return;
    memcpy(E.rbuf + E.rlen, s, n);
    E.rlen += n;
}
static void rb_s(const char* s) {
    rb_app(s, strlen(s));
}
static void rb_c(char c) {
    if (E.rlen + 1 < RBUF_CAP) E.rbuf[E.rlen++] = c;
}

static void rb_flush(void) {
    write(HANDLE_STDOUT, E.rbuf, E.rlen);
    E.rlen = 0;
}


static void mark_lines_dirty_from(int from_buf) {
    for (int y = 0; y < E.edit_rows; y++) {
        if (y + E.rc.row_off >= from_buf)
            E.line_dirty[y] = true;
    }
}

static void mark_line_dirty(int buf_row) {
    int y = buf_row - E.row_off;
    if (y >= 0 && y < E.edit_rows) E.line_dirty[y] = true;
}

static void mark_all_dirty(void) {
    for (int y = 0; y < E.edit_rows; y++) E.line_dirty[y] = true;
}


static void term_restore(void) {
    rb_s(SHOW_CUR CSI "2J" CSI "1;1H" RESET);
    rb_flush();
    tcsetattr(HANDLE_STDIN, &E.orig_mode);
}

static void die(const char* msg) {
    term_restore();
    printf(GL_WARN "  vedit: %s\n", msg);
    exit(1);
}

static void term_init(void) {
    if (tcgetattr(HANDLE_STDIN, &E.orig_mode) < 0) die("tcgetattr failed");
    tty_mode_t raw = {.mode = TTY_MODE_RAW, .echo = 0};
    if (tcsetattr(HANDLE_STDIN, &raw) < 0) die("tcsetattr failed");

    tty_size_t sz;
    if (tty_get_size(HANDLE_STDIN, &sz) < 0 || sz.rows < 4 || sz.cols < 20) {
        sz.rows = 24;
        sz.cols = 80;
    }
    E.rows = sz.rows;
    E.cols = sz.cols;
    E.edit_rows = sz.rows - 3;

    E.line_dirty = malloc(sizeof(bool) * E.edit_rows);
    if (!E.line_dirty) die("OOM (line_dirty)");
    mark_all_dirty();

    E.rc.botbar_rendered = false;
    E.rc.cx       = -1;
    E.rc.cy       = -1;
    E.rc.row_off  = -1;
    E.rc.col_off  = -1;
    E.rc.nlines   = -1;
    E.rc.modified = !E.modified;
}

static int read_byte(void) {
    char c;
    return (read(HANDLE_STDIN, &c, 1) == 1) ? (unsigned char)c : -1;
}

static int read_key(void) {
    int c = read_byte();
    if (c < 0) return KEY_NONE;

    if (c != 0x1B) return c;

    if (tty_has_input(50) <= 0) return 0x1B;

    int c1 = read_byte();
    if (c1 != '[' && c1 != 'O') return 0x1B;

    /* Read CSI body until final byte (0x40-0x7E) */
    char seq[16];
    int slen = 0;
    while (slen < (int)sizeof(seq) - 1) {
        if (tty_has_input(50) <= 0) break;
        int b = read_byte();
        if (b < 0) break;
        seq[slen++] = (char)b;
        if (b >= 0x40 && b <= 0x7E) break;
    }
    seq[slen] = '\0';
    if (slen == 0) return 0x1B;

    if (c1 == 'O') {
        if (seq[0] == 'A') return KEY_UP;
        if (seq[0] == 'B') return KEY_DOWN;
        if (seq[0] == 'C') return KEY_RIGHT;
        if (seq[0] == 'D') return KEY_LEFT;
        if (seq[0] == 'H') return KEY_HOME;
        if (seq[0] == 'F') return KEY_END;
        return 0x1B;
    }

    if (seq[0] == 'A') return KEY_UP;
    if (seq[0] == 'B') return KEY_DOWN;
    if (seq[0] == 'C') return KEY_RIGHT;
    if (seq[0] == 'D') return KEY_LEFT;
    if (seq[0] == 'H') return KEY_HOME;
    if (seq[0] == 'F') return KEY_END;

    if (seq[slen - 1] == '~') {
        int n = 0;
        for (int i = 0; seq[i] && seq[i] != '~'; i++)
            if (seq[i] >= '0' && seq[i] <= '9') n = n * 10 + (seq[i] - '0');
        switch (n) {
            case 1:
            case 7:
                return KEY_HOME;
            case 4:
            case 8:
                return KEY_END;
            case 3:
                return KEY_DEL;
            case 5:
                return KEY_PAGE_UP;
            case 6:
                return KEY_PAGE_DOWN;
        }
    }

    return 0x1B;
}


static void line_grow(Line* l, int need) {
    if (l->cap >= need) return;
    l->cap = (l->cap == 0) ? LINE_INIT_CAP : l->cap * 2;
    while (l->cap < need) l->cap *= 2;
    l->data = realloc(l->data, l->cap);
    if (!l->data) die("OOM");
}

static void line_insert(Line* l, int at, char c) {
    line_grow(l, l->len + 2);
    memmove(l->data + at + 1, l->data + at, l->len - at);
    l->data[at] = c;
    l->data[++l->len] = '\0';
}

static void line_delete(Line* l, int at) {
    if (at < 0 || at >= l->len) return;
    memmove(l->data + at, l->data + at + 1, l->len - at);
    l->data[--l->len] = '\0';
}

static void buf_ensure(void) {
    if (E.nlines < E.lines_cap) return;
    E.lines_cap = E.lines_cap ? E.lines_cap * 2 : 64;
    E.lines = realloc(E.lines, sizeof(Line) * E.lines_cap);
    if (!E.lines) die("OOM");
}

static void buf_insert_line(int at, const char* text, int len) {
    buf_ensure();
    memmove(&E.lines[at + 1], &E.lines[at], sizeof(Line) * (E.nlines - at));
    Line* l = &E.lines[at];
    l->cap = 0;
    l->len = 0;
    l->data = NULL;
    line_grow(l, len + 1);
    memcpy(l->data, text, len);
    l->data[len] = '\0';
    l->len = len;
    E.nlines++;
}

static void buf_remove_line(int at) {
    if (at < 0 || at >= E.nlines) return;
    free(E.lines[at].data);
    memmove(&E.lines[at], &E.lines[at + 1], sizeof(Line) * (E.nlines - at - 1));
    E.nlines--;
}


static void ed_clamp_cx(void) {
    int llen = (E.cy < E.nlines) ? E.lines[E.cy].len : 0;
    if (E.cx > llen) E.cx = llen;
    if (E.cx < 0) E.cx = 0;
}

static void ed_insert_char(char c) {
    if (E.cy == E.nlines) buf_insert_line(E.cy, "", 0);

    if (c == '\t') {
        int sp = TAB_WIDTH - (E.cx % TAB_WIDTH);
        for (int i = 0; i < sp; i++) line_insert(&E.lines[E.cy], E.cx + i, ' ');
        E.cx += sp;
    } else {
        line_insert(&E.lines[E.cy], E.cx++, c);
    }
    E.modified = true;
    mark_line_dirty(E.cy);
}

static void ed_newline(void) {
    if (E.cy == E.nlines) buf_insert_line(E.cy, "", 0);

    Line* cur = &E.lines[E.cy];
    int rest = cur->len - E.cx;

    int indent = 0;
    while (indent < E.cx && cur->data[indent] == ' ') indent++;

    /* New line = indent + text after cursor */
    buf_insert_line(E.cy + 1, cur->data + E.cx, rest);
    cur->len = E.cx;
    cur->data[E.cx] = '\0';

    for (int i = indent - 1; i >= 0; i--) line_insert(&E.lines[E.cy + 1], 0, ' ');

    mark_lines_dirty_from(E.cy);

    E.cy++;
    E.cx = indent;
    E.modified = true;
}

static void ed_backspace(void) {
    if (E.cx == 0 && E.cy == 0) return;

    if (E.cx > 0) {
        line_delete(&E.lines[E.cy], --E.cx);
        mark_line_dirty(E.cy);
    } else {
        Line* prev = &E.lines[E.cy - 1];
        Line* cur = &E.lines[E.cy];
        int pivot = prev->len;
        line_grow(prev, prev->len + cur->len + 1);
        memcpy(prev->data + prev->len, cur->data, cur->len);
        prev->len += cur->len;
        prev->data[prev->len] = '\0';
        buf_remove_line(E.cy--);
        E.cx = pivot;
        mark_lines_dirty_from(E.cy);
    }
    E.modified = true;
}

static void ed_delete_fwd(void) {
    if (E.cy >= E.nlines) return;
    Line* cur = &E.lines[E.cy];

    if (E.cx < cur->len) {
        line_delete(cur, E.cx);
        mark_line_dirty(E.cy);
    } else if (E.cy + 1 < E.nlines) {
        /* Merge with next line */
        Line* next = &E.lines[E.cy + 1];
        line_grow(cur, cur->len + next->len + 1);
        memcpy(cur->data + cur->len, next->data, next->len);
        cur->len += next->len;
        cur->data[cur->len] = '\0';
        buf_remove_line(E.cy + 1);
        mark_lines_dirty_from(E.cy);
    }
    E.modified = true;
}

static void ed_cut(void) {
    if (E.cy >= E.nlines) return;

    free(E.clip);
    Line* l = &E.lines[E.cy];
    E.clip = malloc(l->len + 2);
    if (E.clip) {
        memcpy(E.clip, l->data, l->len);
        E.clip[l->len] = '\n';
        E.clip[l->len + 1] = '\0';
        E.clip_len = l->len + 1;
    }

    buf_remove_line(E.cy);
    if (E.nlines == 0) buf_insert_line(0, "", 0);
    if (E.cy >= E.nlines) E.cy = E.nlines - 1;
    E.cx = 0;
    E.modified = true;
    mark_lines_dirty_from(E.cy);
}

static void ed_paste(void) {
    if (!E.clip || E.clip_len == 0) return;
    int len = E.clip_len;
    /* Strip trailing newline for paste */
    if (len > 0 && E.clip[len - 1] == '\n') len--;
    buf_insert_line(E.cy, E.clip, len);
    E.modified = true;
    mark_lines_dirty_from(E.cy);
}

static const char* const KW[] = {
    "if",       "else",     "for",      "while",     "do",     "switch", "case",     "default",  "break",   "continue",
    "return",   "goto",     "sizeof",   "typedef",   "struct", "union",  "enum",     "static",   "extern",  "const",
    "volatile", "inline",   "register", "auto",      "void",   "NULL",   "true",     "false",    "nullptr", "namespace",
    "class",    "public",   "private",  "protected", "new",    "delete", "template", "typename", "virtual", "override",
    "final",    "explicit", "noexcept", "operator",  "using",  "this",   "throw",    "try",      "catch",   NULL
};

static const char* const TYPES[] = {"int",      "char",      "short",       "long",      "float",      "double",
                                    "unsigned", "signed",    "bool",        "u8",        "u16",        "u32",
                                    "u64",      "i8",        "i16",         "i32",       "i64",        "usize",
                                    "isize",    "uint8_t",   "uint16_t",    "uint32_t",  "uint64_t",   "int8_t",
                                    "int16_t",  "int32_t",   "int64_t",     "size_t",    "ssize_t",    "ptrdiff_t",
                                    "intptr_t", "uintptr_t", "FILE_HANDLE", "HANDLE",    "DIR_HANDLE", "CHANNEL_HANDLE",
                                    "RealmID",  "UnitID",    "RealmId",     "pollhdl_t", NULL};

static bool is_sep(char c) {
    return !c || c == ' ' || c == '\t' || c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']' ||
           c == ';' || c == ',' || c == '.' || c == '*' || c == '+' || c == '-' || c == '/' || c == '=' || c == '<' ||
           c == '>' || c == '&' || c == '|' || c == '!' || c == '~' || c == '^' || c == '%' || c == ':' || c == '?';
}

static bool kw_match(const char* s, int n, const char* const* tab) {
    for (int i = 0; tab[i]; i++) {
        int kn = strlen(tab[i]);
        if (kn == n && strncmp(s, tab[i], n) == 0) return true;
    }
    return false;
}

/* Render one line with syntax highlighting into the render buffer.
   col_off: horizontal scroll offset.  max_cols: visible columns.
   Returns the number of screen columns actually written (for padding). */
static int render_hl(const char* data, int len, bool c_mode, int col_off, int max_cols) {
    const char* cur_col = NULL;
    int scol = 0; /* screen column counter */
    int src = 0;  /* source byte position  */
    bool in_mlc = false;

    {
        int skip = col_off;
        while (src < len && skip > 0) {
            src++;
            skip--;
        }
    }

#define SET_COL(c)            \
    do {                      \
        if (cur_col != (c)) { \
            rb_s(c);          \
            cur_col = (c);    \
        }                     \
    } while (0)

    while (src < len && scol < max_cols) {
        char c = data[src];

        if (!c_mode) {
            SET_COL(SYN_NORM);
            rb_c(c);
            src++;
            scol++;
            continue;
        }

        /* Multi-line comment continuation */
        if (in_mlc) {
            SET_COL(SYN_CMNT);
            rb_c(c);
            if (c == '*' && src + 1 < len && data[src + 1] == '/') {
                rb_c('/');
                src += 2;
                scol += 2;
                in_mlc = false;
            } else {
                src++;
                scol++;
            }
            continue;
        }

        /* Single-line comment */
        if (c == '/' && src + 1 < len && data[src + 1] == '/') {
            SET_COL(SYN_CMNT);
            int avail = len - src;
            int draw = (scol + avail > max_cols) ? (max_cols - scol) : avail;
            rb_app(data + src, draw);
            src += avail;
            scol += avail;
            continue;
        }

        /* Multi-line comment start */
        if (c == '/' && src + 1 < len && data[src + 1] == '*') {
            in_mlc = true;
            SET_COL(SYN_CMNT);
            rb_c(c);
            src++;
            scol++;
            continue;
        }

        /* Preprocessor directive */
        if (c == '#' && (src == 0 || is_sep(data[src - 1]))) {
            SET_COL(SYN_PREP);
            int avail = len - src;
            int draw = (scol + avail > max_cols) ? (max_cols - scol) : avail;
            rb_app(data + src, draw);
            src += avail;
            scol += avail;
            continue;
        }

        /* String / char literal */
        if (c == '"' || c == '\'') {
            char delim = c;
            SET_COL(SYN_STR);
            rb_c(c);
            src++;
            scol++;
            while (src < len && scol < max_cols) {
                char sc = data[src];
                src++;
                scol++;
                rb_c(sc);
                if (sc == '\\' && src < len) {
                    rb_c(data[src]);
                    src++;
                    scol++;
                } else if (sc == delim)
                    break;
            }
            continue;
        }

        /* Number literal */
        if ((c >= '0' && c <= '9') && (src == 0 || is_sep(data[src - 1]))) {
            SET_COL(SYN_NUM);
            while (src < len && scol < max_cols) {
                char d = data[src];
                if (!((d >= '0' && d <= '9') || (d >= 'a' && d <= 'f') || (d >= 'A' && d <= 'F') || d == 'x' ||
                      d == 'X' || d == '.' || d == 'u' || d == 'U' || d == 'l' || d == 'L'))
                    break;
                rb_c(d);
                src++;
                scol++;
            }
            continue;
        }

        /* Identifier / keyword */
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            int s0 = src;
            while (src < len && ((data[src] >= 'a' && data[src] <= 'z') || (data[src] >= 'A' && data[src] <= 'Z') ||
                                 (data[src] >= '0' && data[src] <= '9') || data[src] == '_'))
                src++;
            int tlen = src - s0;
            const char* col;
            if (kw_match(data + s0, tlen, KW))
                col = SYN_KW;
            else if (kw_match(data + s0, tlen, TYPES))
                col = SYN_TYPE;
            else
                col = SYN_NORM;
            SET_COL(col);
            int draw = tlen;
            if (scol + draw > max_cols) draw = max_cols - scol;
            rb_app(data + s0, draw);
            scol += tlen;
            continue;
        }

        SET_COL(SYN_NORM);
        rb_c(c);
        src++;
        scol++;
    }
#undef SET_COL
    rb_s(RESET);
    return scol; /* number of screen columns written */
}

static bool is_c_like(void) {
    int n = strlen(E.filename);
    return (n > 2 && strcmp(E.filename + n - 2, ".c") == 0) || (n > 2 && strcmp(E.filename + n - 2, ".h") == 0) ||
           (n > 4 && strcmp(E.filename + n - 4, ".cpp") == 0) || (n > 4 && strcmp(E.filename + n - 4, ".hpp") == 0) ||
           (n > 3 && strcmp(E.filename + n - 3, ".cc") == 0);
}

static const char* lang_name(void) {
    int n = strlen(E.filename);
    if (n > 4 && strcmp(E.filename + n - 4, ".cpp") == 0) return "C++";
    if (n > 4 && strcmp(E.filename + n - 4, ".hpp") == 0) return "C++ Header";
    if (n > 3 && strcmp(E.filename + n - 3, ".cc") == 0) return "C++";
    if (n > 2 && strcmp(E.filename + n - 2, ".h") == 0) return "C Header";
    if (n > 2 && strcmp(E.filename + n - 2, ".c") == 0) return "C";
    if (n > 3 && strcmp(E.filename + n - 3, ".md") == 0) return "Markdown";
    if (n > 4 && strcmp(E.filename + n - 4, ".txt") == 0) return "Text";
    if (n > 4 && strcmp(E.filename + n - 4, ".asm") == 0) return "ASM";
    if (n > 3 && strcmp(E.filename + n - 3, ".sh") == 0) return "Shell";
    return "Text";
}

/* 4-digit line number + " │ " */
#define GUTTER 7

static void scroll_view(void) {
    ed_clamp_cx();

    /* Vertical */
    if (E.cy < E.row_off) E.row_off = E.cy;
    if (E.cy >= E.row_off + E.edit_rows) E.row_off = E.cy - E.edit_rows + 1;

    /* Horizontal */
    int content_w = E.cols - GUTTER;
    if (E.cx < E.col_off) E.col_off = E.cx;
    if (E.cx >= E.col_off + content_w) E.col_off = E.cx - content_w + 1;
}

static void render_topbar(void) {
    char tmp[128];
    const char* fn = E.filename[0] ? E.filename : "[No Name]";

    /* A: file icon + truncated name */
    rb_s(BG_SB FG_SB_DARK);
    snprintf(tmp, sizeof(tmp), " " GL_FILE "  %.30s ", fn);
    rb_s(tmp);

    /* Arrow A to B */
    rb_s(BG_SB2 FG_SB_BLUE GL_PL_R);

    /* B: modified indicator */
    rb_s(BG_SB2);
    if (E.modified) {
        rb_s("\033[38;2;245;200;60m " GL_PENCIL " Modified ");
    } else {
        rb_s("\033[38;2;80;220;100m " GL_SAVED " Saved    ");
    }


    rb_s(BG_BASE FG_SB_MID);
    rb_s(CSI "K");          /* erase to EOL with BG_BASE active */
    rb_s(BG_BASE GL_PL_R);

    /* Jump to right side */
    snprintf(tmp, sizeof(tmp), CSI "%dG", E.cols - 29);
    rb_s(tmp);

    /* Arrow base to C */
    rb_s(FG_SB_MID BG_BASE GL_PL_L);

    /* C: language + line:col */
    rb_s(BG_SB2 FG_SB_LIGHT);
    snprintf(tmp, sizeof(tmp), " %s  " GL_PL_RT "  %d:%d ", lang_name(), E.cy + 1, E.cx + 1);
    rb_s(tmp);

    /* Arrow C to D */
    rb_s(FG_SB_BLUE BG_SB GL_PL_L);

    /* D: total lines */
    rb_s(BG_SB FG_SB_DARK);
    snprintf(tmp, sizeof(tmp), " " GL_LINES " %d ", E.nlines);
    rb_s(tmp);

    rb_s(BG_SB CSI "K");
    rb_s(RESET);
}

static void render_botbar(void) {
    rb_s(BG_SB FG_SB_DARK);
    rb_s(
        " ^S " GL_SAVE " Save " GL_PL_RT " ^W " GL_SAVEAS " SaveAs " GL_PL_RT " ^Q " GL_QUIT " Quit " GL_PL_RT
        " ^F " GL_FIND " Find " GL_PL_RT " ^G " GL_GOTO " Go-to " GL_PL_RT " ^K " GL_CUT " Cut " GL_PL_RT
        " ^U " GL_PASTE " Paste "
    );
    rb_s(BG_SB CSI "K");
    rb_s(RESET);
}

static void set_msg(bool err, const char* s) {
    strncpy(E.msg, s, sizeof(E.msg) - 1);
    E.msg[sizeof(E.msg) - 1] = '\0';
    E.msg_err = err;
}

#define BG_MSGBAR   "\033[48;2;20;20;32m"    /* deep indigo */
#define FG_MSG_OK   "\033[38;2;100;210;130m" /* green  */
#define FG_MSG_ERR  "\033[38;2;245;100;80m"  /* red-orange */
#define FG_MSG_DIM  "\033[38;2;60;60;85m"    /* muted */
#define GL_INFO     "\xef\x81\xaa"           /* U+F06A nf-fa-exclamation_circle */
#define GL_OK       "\xef\x80\x98"           /* U+F058 nf-fa-check_circle */
#define MSGBAR_PREFIX_COLS 5

static void render(void) {
    scroll_view();

    if (E.row_off != E.rc.row_off || E.col_off != E.rc.col_off) {
        mark_all_dirty();
    }

    mark_line_dirty(E.rc.cy);  /* old cursor row */
    mark_line_dirty(E.cy);     /* new cursor row */

    E.rlen = 0;
    char mvbuf[24];

    rb_s(HIDE_CUR);

    bool topbar_dirty = (E.cx       != E.rc.cx      ||
                         E.cy       != E.rc.cy      ||
                         E.modified != E.rc.modified ||
                         E.nlines   != E.rc.nlines);
    if (topbar_dirty) {
        rb_s(CSI "1;1H");
        render_topbar();
    }

    bool hl = is_c_like();
    int  cw = E.cols - GUTTER;

    for (int y = 0; y < E.edit_rows; y++) {
        /* Skip rows that haven't changed. */
        if (!E.line_dirty[y]) continue;
        E.line_dirty[y] = false;

        int  row = y + E.row_off;
        bool cur = (row == E.cy);

        snprintf(mvbuf, sizeof(mvbuf), CSI "%d;1H", y + 2);
        rb_s(mvbuf);
        rb_s(cur ? BG_CURLINE : BG_BASE);

        if (row >= E.nlines) {
            /* Past EOF: tilde placeholder, pad rest with bg colour. */
            rb_s(FG_DIM "    ~" GL_BOX);
            /* Fill remaining columns so no stale content shows through. */
            int pad = E.cols - 5; /* 5 = "    ~" + glyph (visual width 1) */
            for (int i = 0; i < pad; i++) rb_c(' ');
        } else {
            /* Gutter */
            rb_s(cur ? FG_CURLN : FG_LINENO);
            char lno[8];
            snprintf(lno, sizeof(lno), "%4d ", row + 1);
            rb_s(lno);
            rb_s(FG_DIM GL_BOX);

            /* Content */
            rb_s(cur ? BG_CURLINE : BG_BASE);
            rb_c(' ');

            /* render_hl returns the number of screen columns it wrote. */
            int written = render_hl(E.lines[row].data, E.lines[row].len,
                                    hl, E.col_off, cw - 1);

            rb_s(cur ? BG_CURLINE : BG_BASE);
            const int remaining = (cw - 1) - written;
            for (int i = 0; i < remaining; i++) rb_c(' ');
        }
        rb_s(RESET);
    }

    snprintf(mvbuf, sizeof(mvbuf), CSI "%d;1H", E.rows - 1);
    rb_s(BG_MSGBAR);
    rb_s(mvbuf);
    rb_s(CSI "2K");

    if (E.msg[0]) {
        const char* icon_esc = NULL;
        const char* txt_esc = NULL;
        const char* icon_glyph = NULL;
        if (E.msg_err) {
            icon_esc = FG_MSG_ERR;
            txt_esc  = FG_MSG_ERR;
            icon_glyph = GL_INFO;
        } else if (E.in_prompt) {
            icon_esc   = "\033[38;2;130;160;250m";
            txt_esc    = "\033[38;2;215;215;230m";
            icon_glyph = GL_FIND;
        } else {
            icon_esc   = FG_MSG_OK;
            txt_esc    = FG_MSG_OK;
            icon_glyph = GL_OK;
        }
        rb_s(icon_esc);
        rb_s(" ");
        rb_s(icon_glyph);
        rb_s("  ");
        rb_s(txt_esc);
        rb_s(E.msg);
    }
    rb_s(RESET);

    if (!E.rc.botbar_rendered) {
        snprintf(mvbuf, sizeof(mvbuf), CSI "%d;1H", E.rows);
        rb_s(mvbuf);
        render_botbar();
        E.rc.botbar_rendered = true;
    }

    if (E.in_prompt) {
        int col = E.prompt_col;
        if (col < 1)      col = 1;
        if (col > E.cols) col = E.cols;
        snprintf(mvbuf, sizeof(mvbuf), CSI "%d;%dH", E.rows - 1, col);
    } else {
        int sr = (E.cy - E.row_off) + 2;
        int sc = (E.cx - E.col_off) + GUTTER + 1;
        snprintf(mvbuf, sizeof(mvbuf), CSI "%d;%dH", sr, sc);
    }
    rb_s(mvbuf);
    rb_s(SHOW_CUR);

    rb_flush();

    E.rc.cx      = E.cx;
    E.rc.cy      = E.cy;
    E.rc.row_off = E.row_off;
    E.rc.col_off = E.col_off;
    E.rc.nlines  = E.nlines;
    E.rc.modified = E.modified;
}


static void file_load(const char* path) {
    for (int i = 0; i < E.nlines; i++) free(E.lines[i].data);
    E.nlines = 0;

    HANDLE hdl = open(path, O_RDONLY);
    if (hdl < 0) {
        buf_insert_line(0, "", 0);
        set_msg(false, "New file");
        return;
    }

    const int chunk = 8192;
    char* fb = NULL;
    int fl = 0, fc = 0;
    char tmp[8192];
    int64_t n = 0;

    while ((n = read(hdl, tmp, chunk)) > 0) {
        if (fl + (int)n >= fc) {
            fc = fc ? fc * 2 : chunk * 4;
            if (fl + (int)n >= fc) fc = fl + (int)n + 1;
            fb = realloc(fb, fc);
            if (!fb) {
                close(hdl);
                die("OOM reading file");
            }
        }
        memcpy(fb + fl, tmp, n);
        fl += n;
    }
    close(hdl);

    /* Split on newlines */
    int ls = 0;
    for (int i = 0; i <= fl; i++) {
        if (i == fl || fb[i] == '\n') {
            int ll = i - ls;
            if (ll > 0 && fb[ls + ll - 1] == '\r') ll--; /* strip \r */
            buf_insert_line(E.nlines, fb + ls, ll);
            ls = i + 1;
        }
    }
    if (E.nlines == 0) buf_insert_line(0, "", 0);
    free(fb);

    E.modified = false;
    char msg[80];
    snprintf(msg, sizeof(msg), "Opened \"%s\" — %d lines", path, E.nlines);
    set_msg(false, msg);
    mark_all_dirty();
}

static void file_save(const char* path) {
    if (!path || !path[0]) {
        set_msg(true, "No filename — use ^W to set one");
        return;
    }

    int64_t fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        char buf[100];
        snprintf(buf, sizeof(buf),"Save failed: %s (%d)",strerror(fd), fd);
        set_msg(true, buf);
        return;
    }

    int total = 0;
    for (int i = 0; i < E.nlines; i++) {
        write(fd, E.lines[i].data, E.lines[i].len);
        write(fd, "\n", 1);
        total += E.lines[i].len + 1;
    }
    close(fd);

    E.modified = false;
    char msg[96];
    snprintf(msg, sizeof(msg), "Saved %d bytes to \"%s\"", total, path);
    set_msg(false, msg);
}

/* Returns number of chars entered, or -1 on cancel. */
static int prompt(const char* prefix, char* out, int out_sz) {
    int plen = 0;
    out[0] = '\0';
    E.in_prompt = true;

    while (1) {
        char display[192];
        snprintf(display, sizeof(display), "%s%s", prefix, out);
        set_msg(false, display);

        int prefix_len = (int)strlen(prefix);
        E.prompt_col = MSGBAR_PREFIX_COLS + prefix_len + plen;

        render();

        int c = read_key();

        if (c == '\r' || c == '\n') {
            E.in_prompt = false;
            return plen;
        }
        if (c == 0x1B || c == CTRL('c')) {
            E.in_prompt = false;
            return -1;
        }

        if ((c == 127 || c == CTRL('h')) && plen > 0) {
            out[--plen] = '\0';
        } else if (c >= 0x20 && c < 0x7F && plen < out_sz - 1) {
            out[plen++] = (char)c;
            out[plen]   = '\0';
        }
    }
}

static void cmd_save(void) {
    file_save(E.filename);
}

static void cmd_save_as(void) {
    char newname[MAX_FILENAME];
    strncpy(newname, E.filename, sizeof(newname));
    if (prompt("Save as: ", newname, sizeof(newname)) < 0) {
        set_msg(false, "Save cancelled");
        return;
    }
    if (!newname[0]) return;
    strncpy(E.filename, newname, MAX_FILENAME - 1);
    file_save(E.filename);
}

static void cmd_find(void) {
    char q[128];
    strncpy(q, E.last_query, sizeof(q));
    if (prompt("Find: ", q, sizeof(q)) < 0) {
        set_msg(false, "");
        return;
    }
    if (!q[0]) {
        set_msg(false, "");
        return;
    }
    strncpy(E.last_query, q, sizeof(E.last_query));

    int qn = strlen(q);

    /* Forward search from current position + 1 */
    for (int row = E.cy; row < E.nlines; row++) {
        const Line* l = &E.lines[row];
        int s = (row == E.cy) ? E.cx + 1 : 0;
        for (int col = s; col + qn <= l->len; col++) {
            if (strncmp(l->data + col, q, qn) == 0) {
                E.cy = row;
                E.cx = col;
                set_msg(false, "Match found");
                mark_all_dirty(); /* cursor jumped to possibly distant row */
                return;
            }
        }
    }
    /* Wrap around from top */
    for (int row = 0; row < E.cy; row++) {
        const Line* l = &E.lines[row];
        for (int col = 0; col + qn <= l->len; col++) {
            if (strncmp(l->data + col, q, qn) == 0) {
                E.cy = row;
                E.cx = col;
                set_msg(false, "Match found (wrapped)");
                mark_all_dirty();
                return;
            }
        }
    }
    set_msg(true, "No match found");
}

static void cmd_goto(void) {
    char buf[16] = "";
    if (prompt("Go to line: ", buf, sizeof(buf)) < 0) {
        set_msg(false, "");
        return;
    }

    int n = 0;
    for (int i = 0; buf[i] >= '0' && buf[i] <= '9'; i++) n = n * 10 + (buf[i] - '0');
    if (n < 1) n = 1;
    if (n > E.nlines) n = E.nlines;

    E.cy = n - 1;
    E.cx = 0;
    E.row_off = E.cy - E.edit_rows / 2;
    if (E.row_off < 0) E.row_off = 0;

    char msg[48];
    snprintf(msg, sizeof(msg), "Line %d", n);
    set_msg(false, msg);
    mark_all_dirty();
}

/* Returns false when the editor should exit. */
static bool dispatch(void) {
    int c = read_key();

    /* Any non-quit keypress cancels a pending quit confirmation */
    if (c != CTRL('q')) E.quit_pending = false;

    switch (c) {
        case CTRL('q'):
            if (E.modified && !E.quit_pending) {
                set_msg(true, "Unsaved changes!  ^Q again to force-quit, ^S to save.");
                E.quit_pending = true;
                return true;
            }
            return false;

        case CTRL('s'):
            cmd_save();
            break;
        case CTRL('w'):
            cmd_save_as();
            break;

        case CTRL('f'):
            cmd_find();
            break;
        case CTRL('g'):
            cmd_goto();
            break;

        case CTRL('k'):
            ed_cut();
            set_msg(false, "Line cut  (^U to paste)");
            break;
        case CTRL('u'):
            ed_paste();
            set_msg(false, "Pasted");
            break;

        case CTRL('l'):
            /* Force a full repaint including the botbar. */
            mark_all_dirty();
            E.rc.botbar_rendered = false;
            rb_s(CSI "2J" CSI "H");
            rb_flush();
            set_msg(false, "");
            break;

        case KEY_UP:
            if (E.cy > 0) E.cy--;
            ed_clamp_cx();
            break;
        case KEY_DOWN:
            if (E.cy < E.nlines - 1) E.cy++;
            ed_clamp_cx();
            break;
        case KEY_LEFT:
            if (E.cx > 0) {
                E.cx--;
            } else if (E.cy > 0) {
                E.cy--;
                E.cx = E.lines[E.cy].len;
            }
            break;
        case KEY_RIGHT:
            if (E.cy < E.nlines) {
                if (E.cx < E.lines[E.cy].len)
                    E.cx++;
                else if (E.cy + 1 < E.nlines) {
                    E.cy++;
                    E.cx = 0;
                }
            }
            break;

        case CTRL('a'):
        case KEY_HOME: {
            int first = 0;
            if (E.cy < E.nlines) {
                while (first < E.lines[E.cy].len && E.lines[E.cy].data[first] == ' ') first++;
            }
            E.cx = (E.cx > first) ? first : 0;
            break;
        }
        case CTRL('e'):
        case KEY_END:
            E.cx = (E.cy < E.nlines) ? E.lines[E.cy].len : 0;
            break;

        case KEY_PAGE_UP:
            E.cy -= E.edit_rows;
            if (E.cy < 0) E.cy = 0;
            ed_clamp_cx();
            break;
        case KEY_PAGE_DOWN:
            E.cy += E.edit_rows;
            if (E.cy >= E.nlines) E.cy = E.nlines - 1;
            ed_clamp_cx();
            break;

        case '\r':
        case '\n':
            ed_newline();
            E.msg[0] = '\0';
            break;
        case 127:
        case CTRL('h'):
            ed_backspace();
            E.msg[0] = '\0';
            break;
        case KEY_DEL:
            ed_delete_fwd();
            E.msg[0] = '\0';
            break;

        default:
            if (c >= 0x20 && c < 0x80) {
                ed_insert_char((char)c);
                E.msg[0] = '\0';
            }
            break;
    }

    return true;
}

int main(int argc, const char** argv) {
    E.rbuf = malloc(RBUF_CAP);
    if (!E.rbuf) {
        printf(GL_WARN " vedit: out of memory\n");
        return 1;
    }

    E.lines_cap = 128;
    E.lines = malloc(sizeof(Line) * E.lines_cap);
    if (!E.lines) {
        printf(GL_WARN " vedit: out of memory\n");
        return 1;
    }
    E.nlines = 0;

    if (argc > 1) {
        strncpy(E.filename, argv[1], MAX_FILENAME - 1);
        file_load(argv[1]);
    } else {
        buf_insert_line(0, "", 0);
        set_msg(false, "New buffer  \xe2\x80\x94  ^S save  ^Q quit  ^F find  ^G go-to");
    }

    term_init();

    rb_s(HIDE_CUR CSI "2J" CSI "1;1H");
    rb_flush();

    render();

    while (dispatch()) {
        render();
    }

    term_restore();

    for (int i = 0; i < E.nlines; i++) free(E.lines[i].data);
    free(E.lines);
    free(E.rbuf);
    free(E.clip);
    free(E.line_dirty);

    return 0;
}