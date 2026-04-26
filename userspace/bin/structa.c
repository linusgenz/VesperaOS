// fsd.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 21.04.26.
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
#include <errno.h>
#include <mount.h>
#include <poll.h>
#include <signal.h>
#include <stat.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sysstd.h>
#include <vbus.h>
#include <vespera/handles.h>

#include "log_client.h"

#define MAX_MOUNTS 32
#define POLL_TIMEOUT_MS 2000
#define MNT_BASE "/mnt"
#define MOUNTS_TABLE "/run/mounts/table"
#define MOUNTS_ACTIVE "/run/mounts/active"

typedef struct {
    char dev[64];
    char mountpoint[128];
    char fstype[16];
    int active;
} mount_entry_t;

static mount_entry_t g_mounts[MAX_MOUNTS];
static int g_mount_count = 0;
static volatile int g_running = 1;

static void on_sigterm(int sig) {
    (void)sig;
    g_running = 0;
}

/*
 * Returns 1 if 'name' looks like a partition rather than a whole-disk device.
 *
 * Conventions handled:
 *   sda1, sdb2       — SCSI/SATA/USB partitions
 *   nvme0n1p1        — NVMe partitions (base: nvme0n1)
 *   mmcblk0p1        — eMMC/SD partitions (base: mmcblk0)
 *   vda1             — virtio partitions
 *   hda1             — legacy IDE partitions
 */
static int is_partition(const char* name) {
    size_t len = strlen(name);
    if (len == 0) return 0;

    // NVMe: ends with 'p' followed by one or more digits
    if (strncmp(name, "nvme", 4) == 0) {
        const char* p = strrchr(name, 'p');
        if (p && p != name) {
            const char* d = p + 1;
            while (*d) {
                if (*d < '0' || *d > '9') return 0;
                d++;
            }
            if (d != p + 1) return 1;
        }
        return 0;
    }

    if (strncmp(name, "mmcblk", 6) == 0) {
        const char* p = strrchr(name, 'p');
        if (p && p != name) {
            const char* d = p + 1;
            while (*d) {
                if (*d < '0' || *d > '9') return 0;
                d++;
            }
            if (d != p + 1) return 1;
        }
        return 0;
    }

    if (name[len - 1] >= '1' && name[len - 1] <= '9') {
        for (size_t i = 0; i < len - 1; i++) {
            if (name[i] >= 'a' && name[i] <= 'z') return 1;
        }
    }

    return 0;
}

static int is_already_mounted(const char* dev_path) {
    for (int i = 0; i < g_mount_count; i++) {
        if (g_mounts[i].active && strcmp(g_mounts[i].dev, dev_path) == 0) return 1;
    }
    return 0;
}

static void write_mounts_table(void) {
    FILE* f = fopen(MOUNTS_TABLE, "w");
    if (!f) return;
    for (int i = 0; i < g_mount_count; i++) {
        if (!g_mounts[i].active) continue;
        fprintf(f, "%s %s %s\n", g_mounts[i].dev, g_mounts[i].mountpoint, g_mounts[i].fstype);
    }
    fclose(f);
}

static void mark_active(void) {
    FILE* f = fopen(MOUNTS_ACTIVE, "w");
    if (!f) return;
    fputs("1\n", f);
    fclose(f);
}

typedef enum {
    MOUNT_OK = 0,
    MOUNT_BUSY,    // already mounted (-EBUSY from kernel)
    MOUNT_NO_FS,   // no supported filesystem
    MOUNT_NO_DEV,  // device not found / not ready
    MOUNT_ERR,     // other error
} mount_result_t;

static mount_result_t try_mount(const char* dev, const char* mp) {
    static const char* fstypes[] = {"ext4", "fat32", NULL};

    for (int i = 0; fstypes[i]; i++) {
        int64_t r = mount(dev, mp, fstypes[i], 0);

        if (r == 0) {
            if (g_mount_count < MAX_MOUNTS) {
                mount_entry_t* e = &g_mounts[g_mount_count++];
                strncpy(e->dev, dev, sizeof(e->dev) - 1);
                strncpy(e->mountpoint, mp, sizeof(e->mountpoint) - 1);
                strncpy(e->fstype, fstypes[i], sizeof(e->fstype) - 1);
                e->active = 1;
            }
            return MOUNT_OK;
        }

        int err = (int)-r;
        if (err == EBUSY) {
            if (g_mount_count < MAX_MOUNTS) {
                mount_entry_t* e = &g_mounts[g_mount_count++];
                strncpy(e->dev,        dev,      sizeof(e->dev)        - 1);
                strncpy(e->mountpoint, mp,       sizeof(e->mountpoint) - 1);
                strncpy(e->fstype,     "unknown", sizeof(e->fstype)    - 1);
                e->active = 1;
            }
            return MOUNT_BUSY;
        }        if (err == ENODEV) return MOUNT_NO_DEV;
        // EINVAL / ENOTSUP: wrong filesystem type, try next
    }

    return MOUNT_NO_FS;
}

static void do_mount(const char* dev_path, const char* name) {
    char mp[128];
    snprintf(mp, sizeof(mp), "%s/%s", MNT_BASE, name);
    sys_mkdir((uint64_t)mp, 0, 0, 0, 0, 0);

    switch (try_mount(dev_path, mp)) {
        case MOUNT_OK:
            LOG_INFOF("mounted %s -> %s", dev_path, mp);
            return;
        case MOUNT_BUSY:
            // Already handled by the kernel or a prior scan — not an error.
            break;
        case MOUNT_NO_DEV:
            LOG_WARNF("%s: device not ready", dev_path);
            break;
        case MOUNT_NO_FS:
            LOG_WARNF("%s: no supported filesystem (swap, LVM, or unformatted?)", dev_path);
            break;
        default:
            LOG_WARNF("%s: mount failed", dev_path);
            break;
    }

    sys_rmdir((uint64_t)mp, 0, 0, 0, 0, 0);
}

static void scan_and_mount(void) {
    DIR_HANDLE dh = opendir("/dev");
    if ((int64_t)dh < 0) {
        LOG_ERROR("cannot open /dev");
        return;
    }

    dirent_t ent;
    while (readdir(dh, &ent) > 0) {
        if (strcmp(ent.name, ".") == 0 || strcmp(ent.name, "..") == 0) continue;
        if (ent.type != DT_BLOCKDEV) continue;

        // Skip whole-disk devices — only mount partition-level entries.
        if (!is_partition(ent.name)) continue;

        char dev_path[128];
        snprintf(dev_path, sizeof(dev_path), "/dev/%s", ent.name);

        if (is_already_mounted(dev_path)) continue;

        do_mount(dev_path, ent.name);
    }
    closedir(dh);
}

static void handle_hotplug_add(void) {
    LOG_INFO("hotplug add: rescanning /dev");
    scan_and_mount();
    write_mounts_table();
    if (g_mount_count > 0) mark_active();
}

static void handle_hotplug_remove(const char* dev_path) {
    for (int i = 0; i < g_mount_count; i++) {
        if (!g_mounts[i].active) continue;
        if (strcmp(g_mounts[i].dev, dev_path) != 0) continue;
        umount(g_mounts[i].mountpoint, 0);
        g_mounts[i].active = 0;
        LOGF("unmounted %s", g_mounts[i].mountpoint);
    }
    write_mounts_table();
}

int main(void) {
    log_client_init("structa");
    signal(SIGTERM, on_sigterm);

    LOG_INFO("fsd starting");

    sys_mkdir((uint64_t)MNT_BASE, 0, 0, 0, 0, 0);

    scan_and_mount();

    vbus_subscribe("hotplug", "");

    if (g_mount_count > 0) {
        write_mounts_table();
        mark_active();
    }

    struct pollhdl hdls[1];
    hdls[0].hdl = HANDLE_VBUS;
    hdls[0].events = POLLIN;
    unsigned tick = 0;

    while (g_running) {
        hdls[0].revents = 0;
        poll(hdls, 1, POLL_TIMEOUT_MS);

        for (;;) {
            vbus_header_t hdr;
            char payload[256];
            int r = vbus_recv(&hdr, payload, sizeof(payload) - 1);
            if (r <= 0) break;

            if (strcmp(hdr.interface, "hotplug") != 0) continue;

            if (strcmp(hdr.member, "add") == 0) {
                handle_hotplug_add();
            } else if (strcmp(hdr.member, "remove") == 0) {
                payload[sizeof(payload) - 1] = '\0';
                handle_hotplug_remove(payload);
            }
        }

        if (++tick % 15 == 0) {
            scan_and_mount();
            if (g_mount_count > 0) {
                write_mounts_table();
                mark_active();
            }
            tick = 0;
        }
    }

    LOG_INFO("fsd shutting down");
    return 0;
}
