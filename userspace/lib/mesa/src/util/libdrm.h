/*
 * Copyright © 2023 Google, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * A simple header that either gives you the real libdrm or a no-op shim,
 * depending on whether HAVE_LIBDRM is defined.  This is intended to avoid
 * the proliferation of #ifdef'ery to support environments without libdrm.
 */

#ifndef LIBDRM_H
#define LIBDRM_H

#ifdef HAVE_LIBDRM
#include <xf86drm.h>
#else

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <vespera/dev/lucifer_drm.h>

#define DRM_NODE_PRIMARY 0
#define DRM_NODE_CONTROL 1
#define DRM_NODE_RENDER  2
#define DRM_NODE_MAX     3

#define DRM_BUS_PCI       0
#define DRM_BUS_USB       1
#define DRM_BUS_PLATFORM  2
#define DRM_BUS_HOST1X    3

typedef unsigned int drm_magic_t;

static int
drmGetMagic(int fd, drm_magic_t * magic)
{
  return -EINVAL;
}

typedef struct _drmPciDeviceInfo {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t subvendor_id;
    uint16_t subdevice_id;
    uint8_t revision_id;
} drmPciDeviceInfo, *drmPciDeviceInfoPtr;

#define DRM_PLATFORM_DEVICE_NAME_LEN 512

typedef struct _drmPlatformBusInfo {
    char fullname[DRM_PLATFORM_DEVICE_NAME_LEN];
} drmPlatformBusInfo, *drmPlatformBusInfoPtr;

typedef struct _drmPlatformDeviceInfo {
    char **compatible; /* NULL terminated list of compatible strings */
} drmPlatformDeviceInfo, *drmPlatformDeviceInfoPtr;

#define DRM_HOST1X_DEVICE_NAME_LEN 512

typedef struct _drmHost1xBusInfo {
    char fullname[DRM_HOST1X_DEVICE_NAME_LEN];
} drmHost1xBusInfo, *drmHost1xBusInfoPtr;

typedef struct _drmPciBusInfo {
   uint16_t domain;
   uint8_t bus;
   uint8_t dev;
   uint8_t func;
} drmPciBusInfo, *drmPciBusInfoPtr;

typedef struct _drmDevice {
    char **nodes; /* DRM_NODE_MAX sized array */
    int available_nodes; /* DRM_NODE_* bitmask */
    int bustype;
    union {
       drmPciBusInfoPtr pci;
       drmPlatformBusInfoPtr platform;
       drmHost1xBusInfoPtr host1x;
    } businfo;
    union {
        drmPciDeviceInfoPtr pci;
    } deviceinfo;
    /* ... */
} drmDevice, *drmDevicePtr;

int drmIoctl(int fd, unsigned long request, void *arg);

#define DRM_DEVICE_GET_PCI_REVISION (1 << 0)
static inline int
drmGetDevice2(int fd, uint32_t flags, drmDevicePtr *device)
{
    struct lucifer_query query;
    struct lucifer_query_pci_info info;
    struct stat sbuf;
    drmDevicePtr d = NULL;
    drmPciBusInfoPtr businfo = NULL;
    drmPciDeviceInfoPtr deviceinfo = NULL;
    char *node_path = NULL;
    size_t node_path_len;
    int ret;

    if (device == NULL)
        return -EINVAL;

    if (fd == -1)
        return -EINVAL;

    if (fstat(fd, &sbuf))
        return -errno;

    if (!S_ISCHR(sbuf.st_mode))
        return -EINVAL;

    /* Phase 1: probe required size (matches the two-phase pattern the
     * ioctl uses everywhere else in this driver -- data == 0 means
     * "just tell me the size"). We already know the size at compile
     * time via sizeof(), but go through the same probe/fetch shape as
     * the rest of the query family so a future ABI-incompatible kernel
     * fails loudly instead of silently truncating. */
    memset(&query, 0, sizeof(query));
    query.query = LUCIFER_QUERY_PCI_INFO;
    query.size = 0;
    query.data = 0;

    ret = drmIoctl(fd, LUCIFER_IOCTL_QUERY, &query);
    if (ret != 0)
        return -errno;

    if (query.size != sizeof(info))
        return -EINVAL; /* kernel/userspace ABI mismatch */

    /* Phase 2: fetch. */
    memset(&info, 0, sizeof(info));
    query.size = sizeof(info);
    query.data = (uint64_t)(uintptr_t)&info;

    ret = drmIoctl(fd, LUCIFER_IOCTL_QUERY, &query);
    if (ret != 0)
        return -errno;

    d = calloc(1, sizeof(*d));
    if (!d)
        return -ENOMEM;

    businfo = calloc(1, sizeof(*businfo));
    deviceinfo = calloc(1, sizeof(*deviceinfo));
    if (!businfo || !deviceinfo) {
        ret = -ENOMEM;
        goto err_free;
    }

    /* Bus info: 1:1 field copy, no translation needed. */
    businfo->domain = info.domain;
    businfo->bus = info.bus;
    businfo->dev = info.dev;
    businfo->func = info.func;

    /* Device info: revision + IDs. subvendor/subdevice fields mirror
     * upstream drmPciDeviceInfo naming. */
    deviceinfo->vendor_id = info.vendor_id;
    deviceinfo->device_id = info.device_id;
    deviceinfo->subvendor_id = info.subsystem_vendor_id;
    deviceinfo->subdevice_id = info.subsystem_device_id;
    deviceinfo->revision_id = info.revision;

    d->bustype = DRM_BUS_PCI;
    d->businfo.pci = businfo;
    d->deviceinfo.pci = deviceinfo;

    /* Single node: primary. VesperaOS doesn't split primary/render nodes
     * yet -- one fd, one node, no render-node isolation model to speak
     * of currently. Revisit if/when that changes. */
    d->available_nodes = (1 << DRM_NODE_PRIMARY);
    d->nodes = calloc(DRM_NODE_MAX, sizeof(*d->nodes));
    if (!d->nodes) {
        ret = -ENOMEM;
        goto err_free;
    }

    if (info.name[0] != '\0') {
        /* info.name is e.g. "dri/card0" (DeviceManager-assigned, no
         * leading slash) -- prepend the /dev mount point ourselves. */
        node_path_len = strlen("/dev/") + strlen(info.name) + 1;
        node_path = malloc(node_path_len);
        if (!node_path) {
            ret = -ENOMEM;
            goto err_free;
        }
        snprintf(node_path, node_path_len, "/dev/%s", info.name);
    } else {
        /* Kernel didn't give us a name (shouldn't happen, but don't
         * fail the whole call over a cosmetic field) -- fall back to
         * an empty placeholder rather than leaving nodes[] unset. */
        node_path = strdup("");
        if (!node_path) {
            ret = -ENOMEM;
            goto err_free;
        }
    }

    d->nodes[DRM_NODE_PRIMARY] = node_path;

    *device = d;
    return 0;

err_free:
    free(businfo);
    free(deviceinfo);
    if (d)
        free(d->nodes);
    free(d);
    return ret;
}

static inline int
drmGetDevices2(uint32_t flags, drmDevicePtr devices[], int max_devices)
{
   return -ENOENT;
}

static inline int
drmGetDeviceFromDevId(dev_t dev_id, uint32_t flags, drmDevicePtr *device)
{
   return -ENOENT;
}

static inline void
drmFreeDevice(drmDevicePtr *device) {}

static inline void
drmFreeDevices(drmDevicePtr devices[], int count) {}

static inline char*
drmGetDeviceNameFromFd2(int fd) { return NULL;}

static inline int
drmGetNodeTypeFromFd(int fd)
{
   return -1;
}

static inline char *
drmGetRenderDeviceNameFromFd(int fd)
{
   return NULL;
}

typedef struct _drmVersion {
    int     version_major;        /**< Major version */
    int     version_minor;        /**< Minor version */
    int     version_patchlevel;   /**< Patch level */
    int     name_len;             /**< Length of name buffer */
    char    *name;                /**< Name of driver */
    int     date_len;             /**< Length of date buffer */
    char    *date;                /**< User-space buffer to hold date */
    int     desc_len;             /**< Length of desc buffer */
    char    *desc;                /**< User-space buffer to hold desc */
} drmVersion, *drmVersionPtr;

static inline struct _drmVersion *
    drmGetVersion(int fd) {
    struct lucifer_version kver;
    drmVersionPtr retval;

    memset(&kver, 0, sizeof(kver));

    if (drmIoctl(fd, LUCIFER_IOCTL_VERSION, &kver))
        return NULL;

    retval = calloc(1, sizeof(*retval));
    if (!retval)
        return NULL;

    retval->version_major = kver.version_major;
    retval->version_minor = kver.version_minor;
    retval->version_patchlevel = kver.version_patchlevel;

    retval->name_len = strlen(kver.name);
    retval->name = calloc(1, retval->name_len + 1);
    if (!retval->name)
        goto err_free_retval;
    memcpy(retval->name, kver.name, retval->name_len + 1);

    retval->date_len = strlen(kver.date);
    retval->date = calloc(1, retval->date_len + 1);
    if (!retval->date)
        goto err_free_name;
    memcpy(retval->date, kver.date, retval->date_len + 1);

    retval->desc_len = strlen(kver.desc);
    retval->desc = calloc(1, retval->desc_len + 1);
    if (!retval->desc)
        goto err_free_date;
    memcpy(retval->desc, kver.desc, retval->desc_len + 1);

    return retval;

    err_free_date:
        free(retval->date);
    err_free_name:
        free(retval->name);
    err_free_retval:
        free(retval);
    return NULL;
}


static inline void
drmFreeVersion(struct _drmVersion *v) {
    if (!v)
        return;
    free(v->name);
    free(v->date);
    free(v->desc);
    free(v);
}


#endif

#endif
