// vesfetch.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 24.03.26.
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
#include <fflags.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <vespera/dev/cpuinfo.h>
#include <vespera/dev/ioctl_devinfo.h>
#include <vespera/dev/ioctl_framebuffer.h>
#include <vespera/dev/meminfo.h>

#include "vespera/handles.h"

#define RST   "\033[0m"
#define BD    "\033[1m"
#define LBL   "\033[94m"   /* bright blue */
#define LCY   "\033[96m"   /* bright cyan */
#define WH    "\033[97m"   /* bright white */
#define KEY   LCY BD
#define VAL   WH

#define CF_SSE    (1ULL <<  1)
#define CF_SSE2   (1ULL <<  2)
#define CF_SSE41  (1ULL <<  5)
#define CF_SSE42  (1ULL <<  6)
#define CF_AVX    (1ULL <<  7)
#define CF_AVX2   (1ULL <<  8)
#define CF_FMA    (1ULL <<  9)
#define CF_AVX512 (1ULL << 10)
#define CF_AES    (1ULL << 20)

#define LOGO_LINES 14
#define LOGO_W     30

static const char *LOGO[LOGO_LINES] = {
    "         =======          ",
    "      =======             ",
    "   == =====               ",
    " ====   ==                ",
    " === =====                ",
    "== =======                ",
    " =========                ",
    " ==========               ",
    "== =========              ",
    " === =========            ",
    " ===== ===========   ==== ",
    "   ===== ========  =====  ",
    "     ====  ====  =====    ",
    "         ==    ===        ",
};

#define MAX_INFO 22
#define KEY_W    16

typedef struct {
    char key[32];
    char val[160];
    int  raw;
} InfoLine;

static InfoLine INFO[MAX_INFO];
static int      N_INFO = 0;

static void add(const char *k, const char *v, int raw)
{
    if (N_INFO >= MAX_INFO) return;
    snprintf(INFO[N_INFO].key, sizeof(INFO[N_INFO].key), "%s", k);
    snprintf(INFO[N_INFO].val, sizeof(INFO[N_INFO].val), "%s", v);
    INFO[N_INFO].raw = raw;
    N_INFO++;
}

static int utf8_len(const char *s)
{
    int len = 0;
    while (*s) {
        if ((*s & 0xC0) != 0x80) len++;
        s++;
    }
    return len;
}

static uint64_t read_u64_dev(const char *path)
{
    int64_t h = open(path, O_RDONLY);
    if (h < 0) return 0;
    uint64_t v = 0;
    read(h, &v, sizeof(v));
    close(h);
    return v;
}

static void fmt_size(uint64_t bytes, char *out, size_t n)
{
    double gib = (double)bytes / (1024.0 * 1024.0 * 1024.0);
    if (gib >= 1.0)
        snprintf(out, n, "%.1f GiB", gib);
    else
        snprintf(out, n, "%llu MiB", (unsigned long long)(bytes >> 20));
}

static void fmt_uptime(uint64_t ms, char *out, size_t n)
{
    uint64_t s = ms / 1000;
    uint64_t m = s / 60;  s %= 60;
    uint64_t h = m / 60;  m %= 60;
    uint64_t d = h / 24;  h %= 24;

    if (d)
        snprintf(out, n, "%llud %02lluh %02llum",
                 (unsigned long long)d,
                 (unsigned long long)h,
                 (unsigned long long)m);
    else if (h)
        snprintf(out, n, "%lluh %02llum %02llus",
                 (unsigned long long)h,
                 (unsigned long long)m,
                 (unsigned long long)s);
    else
        snprintf(out, n, "%llum %02llus",
                 (unsigned long long)m,
                 (unsigned long long)s);
}

static void cpu_feats(uint64_t f, char *out, size_t n)
{
    const char *top = "";
    if      (f & CF_AVX512) top = "AVX-512";
    else if (f & CF_AVX2)   top = "AVX2";
    else if (f & CF_AVX)    top = "AVX";
    else if (f & CF_SSE42)  top = "SSE4.2";
    else if (f & CF_SSE41)  top = "SSE4.1";
    else if (f & CF_SSE2)   top = "SSE2";
    else if (f & CF_SSE)    top = "SSE";

    char extra[48] = "";
    if (f & CF_FMA) strncat(extra, " FMA",    sizeof(extra) - strlen(extra) - 1);
    if (f & CF_AES) strncat(extra, " AES-NI", sizeof(extra) - strlen(extra) - 1);

    if (*top && *extra)  snprintf(out, n, "%s%s", top, extra);
    else if (*top)       snprintf(out, n, "%s", top);
    else                 out[0] = '\0';
}

static void list_block_devs(char *out, size_t n)
{
    const int64_t d = opendir("/dev/");
    if (d < 0) { snprintf(out, n, "unknown"); return; }

    out[0] = '\0';
    dirent_t ent;
    int first = 1, cnt = 0;

    while (readdir(d, &ent) > 0 && cnt < 8) {
        if (ent.type != DT_BLOCKDEV) continue;
        if (!first) strncat(out, ", ", n - strlen(out) - 1);
        strncat(out, ent.name, n - strlen(out) - 1);
        first = 0;
        cnt++;
    }
    closedir(d);

    if (!out[0]) snprintf(out, n, "none");
}

int main(void)
{

    cpu_info_t cpu;
    memset(&cpu, 0, sizeof(cpu));
    {
        int64_t h = open("/dev/cpuinfo", O_RDONLY);
        int ok = (h >= 0) &&
                 (read(h, &cpu, sizeof(cpu)) == (ssize_t)sizeof(cpu));
        if (h >= 0) close(h);
        if (!ok) snprintf(cpu.brand, sizeof(cpu.brand), "Unknown");
    }

    meminfo_t mem;
    memset(&mem, 0, sizeof(mem));
    int has_mem = 0;
    {
        int64_t h = open("/dev/meminfo", O_RDONLY);
        has_mem = (h >= 0) &&
                  (read(h, &mem, sizeof(mem)) == (ssize_t)sizeof(mem));
        if (h >= 0) close(h);
    }

    uint64_t uptime_ms = read_u64_dev("/dev/uptime");

    char gpu_str[200];
    snprintf(gpu_str, sizeof(gpu_str), "Unknown");
    {
        int64_t h = open("/dev/gpu", O_RDONLY);
        if (h >= 0) {
            devinfo_t di;
            memset(&di, 0, sizeof(di));
            if (ioctl((uint64_t)h, IOCTL_DEVINFO_GET_ALL, &di) == 0) {
                if (di.vendor[0] && di.model[0])
                    snprintf(gpu_str, sizeof(gpu_str),
                             "%s %s", di.vendor, di.model);
                else if (di.model[0])
                    snprintf(gpu_str, sizeof(gpu_str), "%s", di.model);
                else if (di.vendor[0])
                    snprintf(gpu_str, sizeof(gpu_str), "%s", di.vendor);
            }
            close(h);
        }
    }

    fb_info_t fb;
    memset(&fb, 0, sizeof(fb));
    int has_fb = 0;
    {
        int64_t dd = opendir("/dev/");
        if (dd >= 0) {
            dirent_t ent;
            while (readdir(dd, &ent) > 0) {
                if (strncmp(ent.name, "fb", 2) != 0) continue;
                char path[64];
                snprintf(path, sizeof(path), "/dev/%s", ent.name);
                int64_t hf = open(path, O_RDONLY);
                if (hf >= 0) {
                    has_fb = (ioctl((uint64_t)hf,
                                    FB_IOCTL_GET_INFO, &fb) == 0);
                    close(hf);
                }
                break;
            }
            closedir(dd);
        }
    }

    tty_size_t tsz = {0, 0};
    tty_get_size(HANDLE_STDIN, &tsz);


    const char *user = getenv("USER");
    if (!user || !user[0]) user = "user";

    {
        char hdr[128];
        snprintf(hdr, sizeof(hdr),
                 KEY "%s" RST "@" LBL "vespera" RST, user);
        add("\uf007", hdr, 0);
    }

    {
        int sl = (int)(strlen(user) + 1 + 7);
        if (sl > 60) sl = 60;
        char dashes[64];
        memset(dashes, '-', (size_t)sl);
        dashes[sl] = '\0';
        char colored[96];
        snprintf(colored, sizeof(colored), LBL "-----------------%s" RST, dashes);
        add("", colored, 1);
    }

    add("\uf013 OS",       "VesperaOS x86_64", 0);

    {
        char feat[64] = "";
        cpu_feats(cpu.features, feat, sizeof(feat));
        char val[160];
        if (feat[0])
            snprintf(val, sizeof(val), "%s (%s)", cpu.brand, feat);
        else
            snprintf(val, sizeof(val), "%s", cpu.brand);
        add("\uf4bc CPU", val, 0);
    }

    add("\U000f08ae GPU", gpu_str, 0);

    if (has_mem) {
        char used[24], total[24];
        fmt_size(mem.used_ram,  used,  sizeof(used));
        fmt_size(mem.total_ram, total, sizeof(total));
        char val[64];
        snprintf(val, sizeof(val), "%s / %s", used, total);
        add("\uefc5 Memory", val, 0);
    }

    if (has_fb) {
        char val[48];
        snprintf(val, sizeof(val), "%ux%u px",
                 (unsigned)fb.width, (unsigned)fb.height);
        add("\uf108 Display", val, 0);
    }

    {
        char disks[160];
        list_block_devs(disks, sizeof(disks));
        add("\uf0a0 Disks", disks, 0);
    }

    {
        const char *sh = getenv("SHELL");
        add("\ue795 Shell", sh ? sh : "vsh", 0);
    }

    {
        char val[64];
        if (tsz.cols > 0)
            snprintf(val, sizeof(val), "VesperaOS TTY (%ux%u)",
                     (unsigned)tsz.cols, (unsigned)tsz.rows);
        else
            snprintf(val, sizeof(val), "VesperaOS TTY");
        add("\uf489 Terminal", val, 0);
    }

    if (uptime_ms > 0) {
        char val[48];
        fmt_uptime(uptime_ms, val, sizeof(val));
        add("\uf017 Uptime", val, 0);
    }

    int rows = (N_INFO > LOGO_LINES) ? N_INFO : LOGO_LINES;
    printf("\n");

    for (int i = 0; i < rows; i++) {

        if (i < LOGO_LINES)
            printf(LBL BD " %-*s" RST, LOGO_W, LOGO[i]);
        else
            printf(" %*s", LOGO_W, "");

        /* Right column: info */
        if (i < N_INFO) {
            InfoLine *l = &INFO[i];
            if (l->raw) {
                /* header / separator */
                printf("  %s\n", l->val);
            } else {
                int visible = utf8_len(l->key);
                int pad = KEY_W - visible;
                if (pad < 0) pad = 0;

                printf("  " KEY "%s" RST, l->key);
                for (int i = 0; i < pad; i++) putchar(' ');
                printf(" " VAL "%s" RST "\n", l->val);
            }
        } else {
            printf("\n");
        }
    }

    printf("\n%*s", LOGO_W + 2, "");

    static const char *pal[8] = {
        "\033[40m", "\033[41m", "\033[42m", "\033[43m",
        "\033[44m", "\033[45m", "\033[46m", "\033[47m",
    };
    static const char *pal_bright[8] = {
        "\033[100m", "\033[101m", "\033[102m", "\033[103m",
        "\033[104m", "\033[105m", "\033[106m", "\033[107m",
    };

    for (int c = 0; c < 8; c++) printf("%s   " RST, pal[c]);
    printf("\n%*s", LOGO_W + 2, "");
    for (int c = 0; c < 8; c++) printf("%s   " RST, pal_bright[c]);
    printf("\n\n");

    return 0;
}