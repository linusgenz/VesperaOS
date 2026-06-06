// astra.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 29.05.26.
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

#include "astra.h"

#include <dirent.h>
#include <realm.h>
#include <stdio.h>
#include <stdlib.h>
#include <stella.h>
#include <string.h>

#include "../theme/theme.h"
#include "desktop.h"

#define ASTRA_DIR "/etc/astra"
#define ASTRA_EXT ".astrum"
#define ASTRA_LINE_MAX 256
#define ASTRA_PATH_MAX 128
#define ASTRA_NAME_MAX 64
#define ASTRA_ICON_DIR "/usr/share/icons"
#define ASTRA_ICON_EXT ".png"

typedef struct {
    char bin[ASTRA_PATH_MAX];
} astra_launcher_t;

static astra_launcher_t g_launchers[DESKTOP_ICON_MAX];
static int g_launcher_count = 0;

static void astra_launch(void *user_data) {
    astra_launcher_t *ctx = user_data;
    const char *argv[] = {ctx->bin, NULL};
    const char *envp[] = {"PATH=/bin", NULL};
    int64_t rid = spawn_realm(ctx->bin, (char **)argv, (char **)envp, NULL);
    if (rid < 0) printf("astra: failed to launch %s (%lld)\n", ctx->bin, (long long)rid);
}

/* -------------------------------------------------------------------------
 * .astrum parser
 * ------------------------------------------------------------------------- */

typedef struct {
    char name[ASTRA_NAME_MAX];
    char bin[ASTRA_PATH_MAX];
    char icon[ASTRA_NAME_MAX];
    stella_color_t color;
    bool has_name;
    bool has_bin;
    bool has_icon;
} astrum_entry_t;

static void entry_init(astrum_entry_t *e) {
    memset(e, 0, sizeof(*e));
    e->color = VESPERA_COL(VESPERA_BLUE);
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) *--end = '\0';
    return s;
}

static void parse_line(const char *raw, astrum_entry_t *e) {
    char line[ASTRA_LINE_MAX];
    strncpy(line, raw, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    char *p = trim(line);
    if (*p == '\0' || *p == '#') return;

    char *eq = strchr(p, '=');
    if (!eq) return;
    *eq = '\0';
    char *key = trim(p);
    char *val = trim(eq + 1);

    if (strcmp(key, "name") == 0) {
        strncpy(e->name, val, sizeof(e->name) - 1);
        e->has_name = true;
    } else if (strcmp(key, "bin") == 0) {
        strncpy(e->bin, val, sizeof(e->bin) - 1);
        e->has_bin = true;
    } else if (strcmp(key, "icon") == 0) {
        strncpy(e->icon, val, sizeof(e->icon) - 1);
        e->has_icon = true;
    } else if (strcmp(key, "color") == 0) {
        const char *hex_str = (val[0] == '0' && (val[1] == 'x' || val[1] == 'X')) ? val + 2 : val;
        char *end;
        unsigned long hex = strtoul(hex_str, &end, 16);
        if (end != hex_str)
            e->color = stella_hex((uint32_t)hex & 0x00FFFFFFu);
        else
            printf("astra: invalid color '%s', using default\n", val);
    }
    /* Unknown keys silently ignored. */
}

static bool parse_file(const char *path, astrum_entry_t *e) {
    entry_init(e);

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("astra: cannot open %s\n", path);
        return false;
    }

    char line[ASTRA_LINE_MAX];
    while (fgets(line, sizeof(line), f)) parse_line(line, e);
    fclose(f);

    if (!e->has_name) {
        printf("astra: %s: missing 'name', skipping\n", path);
        return false;
    }
    if (!e->has_bin) {
        printf("astra: %s: missing 'bin', skipping\n", path);
        return false;
    }
    return true;
}

/* -------------------------------------------------------------------------
 * Stable name storage
 * ------------------------------------------------------------------------- */

#define NAME_POOL_SZ (DESKTOP_ICON_MAX * ASTRA_NAME_MAX)
static char g_name_pool[NAME_POOL_SZ];
static int g_name_used = 0;

static const char *name_store(const char *name) {
    int len = (int)strlen(name) + 1;
    if (g_name_used + len > NAME_POOL_SZ) return NULL;
    char *slot = g_name_pool + g_name_used;
    memcpy(slot, name, (size_t)len);
    g_name_used += len;
    return slot;
}

/* Icon path pool — stores "/usr/share/icons/<name>.png" strings. */
#define ICON_PATH_SZ (DESKTOP_ICON_MAX * (ASTRA_NAME_MAX + 32))
static char g_icon_pool[ICON_PATH_SZ];
static int g_icon_used = 0;

/* Returns a stable "/usr/share/icons/<name>.png" string, or NULL. */
static const char *icon_path_store(const char *name) {
    char tmp[ASTRA_NAME_MAX + 32];
    int n = snprintf(tmp, sizeof(tmp), "%s/%s%s", ASTRA_ICON_DIR, name, ASTRA_ICON_EXT);
    if (n < 0 || n >= (int)sizeof(tmp)) return NULL;
    int len = n + 1;
    if (g_icon_used + len > ICON_PATH_SZ) return NULL;
    char *slot = g_icon_pool + g_icon_used;
    memcpy(slot, tmp, (size_t)len);
    g_icon_used += len;
    return slot;
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

int desktop_astra_load(void) {
    DIR_HANDLE dir = opendir(ASTRA_DIR);
    if (!dir) {
        printf("astra: cannot open %s\n", ASTRA_DIR);
        return -1;
    }

    int loaded = 0;
    dirent_t ent;

    while ((readdir(dir, &ent)) != 0) {
        const char *fname = ent.name;
        size_t flen = strlen(fname);
        size_t extlen = strlen(ASTRA_EXT);
        if (flen <= extlen || strcmp(fname + flen - extlen, ASTRA_EXT) != 0) continue;

        char path[ASTRA_PATH_MAX];
        if (snprintf(path, sizeof(path), "%s/%s", ASTRA_DIR, fname) >= (int)sizeof(path)) {
            printf("astra: path too long for %s, skipping\n", fname);
            continue;
        }

        astrum_entry_t entry;
        if (!parse_file(path, &entry)) continue;

        if (g_launcher_count >= DESKTOP_ICON_MAX) {
            printf("astra: icon limit reached, stopping\n");
            break;
        }

        astra_launcher_t *launcher = &g_launchers[g_launcher_count++];
        strncpy(launcher->bin, entry.bin, sizeof(launcher->bin) - 1);

        const char *name = name_store(entry.name);
        if (!name) {
            printf("astra: name pool exhausted, skipping %s\n", fname);
            g_launcher_count--;
            continue;
        }

        const char *icon_path = entry.has_icon ? icon_path_store(entry.icon) : NULL;

        desktop_icon_t icon = {
            .label = name,
            .icon_src = icon_path,
            .icon_color = entry.color,
            .on_launch = astra_launch,
            .launch_data = launcher,
        };

        if (!desktop_add_icon(&icon)) {
            g_launcher_count--;
            break;
        }
        loaded++;
    }

    closedir(dir);
    printf("astra: loaded %d icon(s) from %s\n", loaded, ASTRA_ICON_DIR);
    return loaded;
}
