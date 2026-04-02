// cal.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 31.03.26.
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
#include <time.h>

#define MWIDTH 20
#define MROWS 8
#define GAP 2

static const int MDAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const char *MNAMES[12] = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};

static int is_leap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}
static int month_len(int m, int y) {
    return (m == 2 && is_leap(y)) ? 29 : MDAYS[m - 1];
}
/* Returns 0=Mon .. 6=Su for the first day of month m, year y */
static int first_dow(int m, int y) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y--;
    int dow = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + 1) % 7;
    return (dow + 6) % 7;
}

typedef struct {
    char line[MROWS][MWIDTH + 1]; /* plain text, space-padded, NUL-term */
    int hi_row, hi_off;           /* row + byte-offset of today (-1=none) */
} MonthBuf;

static void render_month(int m, int y, int today_m, int today_y, int today_d, MonthBuf *out) {
    int ri, ci;

    for (ri = 0; ri < MROWS; ri++) {
        memset(out->line[ri], ' ', MWIDTH);
        out->line[ri][MWIDTH] = '\0';
    }
    out->hi_row = out->hi_off = -1;

    int grid[6][7];
    for (ri = 0; ri < 6; ri++) {
        for (ci = 0; ci < 7; ci++) {
            grid[ri][ci] = 0;
        }
    }

    int day = 1, days = month_len(m, y);
    int r = 0, c = first_dow(m, y);
    while (day <= days) {
        grid[r][c] = day++;
        if (++c == 7) {
            c = 0;
            r++;
        }
    }

    char title[32];
    snprintf(title, sizeof(title), "%s %d", MNAMES[m - 1], y);
    int tlen = (int)strlen(title);
    int tpad = (MWIDTH - tlen) / 2;
    memcpy(out->line[0] + tpad, title, tlen);

    memcpy(out->line[1], "Mo Tu We Th Fr Sa Su", MWIDTH);

    int row = 2;
    for (ri = 0; ri < 6; ri++) {
        int any = 0;
        for (ci = 0; ci < 7; ci++)
            if (grid[ri][ci]) {
                any = 1;
                break;
            }
        if (!any) break;

        for (ci = 0; ci < 7; ci++) {
            int d = grid[ri][ci];
            int off = ci * 3;
            if (d > 0) {
                out->line[row][off] = (d >= 10) ? ('0' + d / 10) : ' ';
                out->line[row][off + 1] = '0' + d % 10;
                if (m == today_m && y == today_y && d == today_d) {
                    out->hi_row = row;
                    out->hi_off = off;
                }
            }
        }
        row++;
    }
}

/* Print one line of a MonthBuf, with ANSI highlight for today */
static void print_line(const MonthBuf *mb, int row) {
    const char *s = mb->line[row];
    if (mb->hi_row != row || mb->hi_off < 0) {
        /* no highlight on this row – just dump it */
        int i;
        for (i = 0; i < MWIDTH; i++) putchar(s[i]);
        return;
    }
    int i;
    for (i = 0; i < mb->hi_off; i++) putchar(s[i]);
    printf("\033[7m%c%c\033[0m", s[mb->hi_off], s[mb->hi_off + 1]);
    for (i = mb->hi_off + 2; i < MWIDTH; i++) putchar(s[i]);
}

static void print_single(int m, int y, int tm, int ty, int td) {
    MonthBuf mb;
    render_month(m, y, tm, ty, td, &mb);
    for (int r = 0; r < MROWS; r++) {
        print_line(&mb, r);
        putchar('\n');
    }
}

static void print_three(int m, int y, int tm, int ty, int td) {
    int m1 = m - 1, y1 = y;
    if (m1 < 1) {
        m1 = 12;
        y1--;
    }
    int m3 = m + 1, y3 = y;
    if (m3 > 12) {
        m3 = 1;
        y3++;
    }

    MonthBuf mb[3];
    render_month(m1, y1, tm, ty, td, &mb[0]);
    render_month(m, y, tm, ty, td, &mb[1]);
    render_month(m3, y3, tm, ty, td, &mb[2]);

    for (int r = 0; r < MROWS; r++) {
        print_line(&mb[0], r);
        printf("%*s", GAP, "");
        print_line(&mb[1], r);
        printf("%*s", GAP, "");
        print_line(&mb[2], r);
        putchar('\n');
    }
}

static void print_year(int y, int tm, int ty, int td) {
    int total_width = MWIDTH * 3 + GAP * 2;
    char title[16];
    snprintf(title, sizeof(title), "%d", y);
    int tlen = (int)strlen(title);
    int tpad = (total_width - tlen) / 2;
    printf("%*s%s\n\n", tpad, "", title);

    for (int q = 0; q < 4; q++) {
        MonthBuf mb[3];
        for (int i = 0; i < 3; i++) render_month(q * 3 + i + 1, y, tm, ty, td, &mb[i]);

        for (int r = 0; r < MROWS; r++) {
            print_line(&mb[0], r);
            printf("%*s", GAP, "");
            print_line(&mb[1], r);
            printf("%*s", GAP, "");
            print_line(&mb[2], r);
            putchar('\n');
        }
        if (q < 3) putchar('\n');
    }
}

static void usage(void) {
    puts("Usage: cal [OPTIONS] [[month] year]");
    puts("       cal --help");
    puts("");
    puts("Display a calendar.");
    puts("");
    puts("Options:");
    puts("  -1          Show one month (default)");
    puts("  -3          Show previous, current and next month");
    puts("  -y          Show all months of the year");
    puts("  -m MONTH    Specify month (1-12 or name)");
    puts("  -Y YEAR     Specify year");
    puts("");
    puts("Arguments:");
    puts("  month year  Show given month of given year");
    puts("  year        Show all months of given year");
}

static int parse_month_name(const char *s) {
    static const char *short_names[] = {
        "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"
    };
    char buf[4] = {0};
    for (int i = 0; i < 3 && s[i]; i++) buf[i] = (s[i] >= 'A' && s[i] <= 'Z') ? s[i] + 32 : s[i];
    for (int i = 0; i < 12; i++)
        if (strcmp(buf, short_names[i]) == 0) return i + 1;
    return -1;
}

static int parse_uint(const char *s) {
    int v = 0;
    for (int i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') return -1;
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

int main(int argc, char **argv) {
    int today_m = 1, today_y = 1970, today_d = 1;
    timespec_t ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        time_t t = (time_t)ts.tv_sec;
        struct tm *now = gmtime(&t);
        if (now) {
            today_d = now->tm_mday;
            today_m = now->tm_mon + 1;
            today_y = now->tm_year + 1900;
        }
    }

    int show_year = 0;
    int show_three = 0;
    int req_month = today_m;
    int req_year = today_y;
    int npos = 0, pos[2] = {0, 0};

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            usage();
            return 0;
        } else if (strcmp(a, "-1") == 0) {
            show_three = show_year = 0;
        } else if (strcmp(a, "-3") == 0) {
            show_three = 1;
            show_year = 0;
        } else if (strcmp(a, "-y") == 0) {
            show_year = 1;
            show_three = 0;
        } else if (strcmp(a, "-m") == 0) {
            if (++i >= argc) {
                puts("cal: -m requires an argument");
                return 1;
            }
            int mv = parse_uint(argv[i]);
            if (mv < 0) mv = parse_month_name(argv[i]);
            if (mv < 1 || mv > 12) {
                printf("cal: invalid month '%s'\n", argv[i]);
                return 1;
            }
            req_month = mv;
        } else if (strcmp(a, "-Y") == 0) {
            if (++i >= argc) {
                puts("cal: -Y requires an argument");
                return 1;
            }
            int yv = parse_uint(argv[i]);
            if (yv <= 0) {
                printf("cal: invalid year '%s'\n", argv[i]);
                return 1;
            }
            req_year = yv;
        } else if (a[0] == '-') {
            printf("cal: unknown option '%s'\n", a);
            return 1;
        } else {
            if (npos < 2) {
                pos[npos] = parse_uint(a);
                if (pos[npos] < 0) {
                    printf("cal: invalid argument '%s'\n", a);
                    return 1;
                }
                npos++;
            }
        }
    }

    if (npos == 1) {
        /* single number → treat as year, show whole year */
        req_year = pos[0];
        show_year = 1;
        show_three = 0;
    } else if (npos == 2) {
        req_month = pos[0];
        req_year = pos[1];
        if (req_month < 1 || req_month > 12) {
            printf("cal: invalid month %d\n", req_month);
            return 1;
        }
    }

    if (show_year)
        print_year(req_year, today_m, today_y, today_d);
    else if (show_three)
        print_three(req_month, req_year, today_m, today_y, today_d);
    else
        print_single(req_month, req_year, today_m, today_y, today_d);

    return 0;
}