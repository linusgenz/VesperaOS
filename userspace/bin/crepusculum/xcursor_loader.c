// xcursor_loader.c
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

#include "xcursor_loader.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XCURSOR_MAGIC UINT32_C(0x72756358) /* "Xcur" little-endian */
#define XCURSOR_FILE_VERSION UINT32_C(0x00010000)

#define XCURSOR_CHUNK_TYPE_IMAGE UINT32_C(0xfffd0002)
#define XCURSOR_CHUNK_TYPE_COMMENT UINT32_C(0xfffe0001)

#define XCURSOR_FILE_HEADER_LEN (4 * 4)  /* magic+header+version+ntoc */
#define XCURSOR_TOC_ENTRY_LEN (3 * 4)    /* type+subtype+position     */
#define XCURSOR_IMAGE_HEADER_LEN (9 * 4) /* chunk-header(4×4) + w+h+xhot+yhot+delay */

#define XCURSOR_MAX_DIM UINT32_C(0x7fff)
#define XCURSOR_MAX_NTOC UINT32_C(0x4000) /* Sanity-Limit: 16 384 TOC-Einträge */

#ifndef XCURSOR_DEBUG
#define XCURSOR_DEBUG 1
#endif

#if XCURSOR_DEBUG
#define XCURSOR_DBG(fmt, ...) fprintf(stderr, "[xcursor] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define XCURSOR_DBG_ERRNO(fmt, ...)                                                                                    \
    fprintf(                                                                                                           \
        stderr, "[xcursor] %s:%d: " fmt " (errno=%d: %s)\n", __FILE__, __LINE__, ##__VA_ARGS__, errno, strerror(errno) \
    )
#else
#define XCURSOR_DBG(fmt, ...) ((void)0)
#define XCURSOR_DBG_ERRNO(fmt, ...) ((void)0)
#endif

typedef struct {
    uint32_t type;
    uint32_t subtype;  /* für Images: nominale Größe */
    uint32_t position; /* absoluter Byte-Offset in der Datei */
} xcursor_toc_entry_t;

static bool xcursor_read_exact(FILE* fp, void* buf, size_t n) {
    size_t got = fread(buf, 1, n, fp);
    if (got != n) {
        XCURSOR_DBG_ERRNO("fread: wanted %zu bytes, got %zu", n, got);
        return false;
    }
    return true;
}

static bool xcursor_read_u32(FILE* fp, uint32_t* out) {
    uint8_t b[4];
    if (!xcursor_read_exact(fp, b, 4)) {
        /* xcursor_read_exact already printed */
        return false;
    }
    *out = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return true;
}

static uint32_t xcursor_u32_abs_diff(uint32_t a, uint32_t b) {
    return a >= b ? a - b : b - a;
}

bool xcursor_load_file(const char* filename, uint32_t target_size, loaded_cursor_t* out_cursor) {
    if (!filename || !out_cursor) {
        XCURSOR_DBG("invalid arguments: filename=%p out_cursor=%p", (const void*)filename, (const void*)out_cursor);
        return false;
    }

    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        XCURSOR_DBG_ERRNO("fopen(\"%s\", \"rb\") failed", filename);
        return false;
    }

    bool ok = false;
    xcursor_toc_entry_t* toc = NULL;

    uint32_t magic, file_header_len, version, ntoc;

    if (!xcursor_read_u32(fp, &magic)) {
        XCURSOR_DBG("failed to read magic");
        goto cleanup;
    }
    if (!xcursor_read_u32(fp, &file_header_len)) {
        XCURSOR_DBG("failed to read file_header_len");
        goto cleanup;
    }
    if (!xcursor_read_u32(fp, &version)) {
        XCURSOR_DBG("failed to read version");
        goto cleanup;
    }
    if (!xcursor_read_u32(fp, &ntoc)) {
        XCURSOR_DBG("failed to read ntoc");
        goto cleanup;
    }

    if (magic != XCURSOR_MAGIC) {
        XCURSOR_DBG("bad magic: expected 0x%08x, got 0x%08x", XCURSOR_MAGIC, magic);
        goto cleanup;
    }
    if (file_header_len < XCURSOR_FILE_HEADER_LEN) {
        XCURSOR_DBG("file_header_len too small: %u < %u", file_header_len, XCURSOR_FILE_HEADER_LEN);
        goto cleanup;
    }
    if (ntoc == 0 || ntoc > XCURSOR_MAX_NTOC) {
        XCURSOR_DBG("ntoc out of range: %u (max %u)", ntoc, XCURSOR_MAX_NTOC);
        goto cleanup;
    }

    if (file_header_len > XCURSOR_FILE_HEADER_LEN) {
        long skip = (long)(file_header_len - XCURSOR_FILE_HEADER_LEN);
        XCURSOR_DBG("skipping %ld extra header bytes", skip);
        if (fseek(fp, skip, SEEK_CUR) != 0) {
            XCURSOR_DBG_ERRNO("fseek(skip extra header, %ld) failed", skip);
            goto cleanup;
        }
    }

    /* ------------------------------------------------------------------
     * Read TOC
     *    every entry: type(4) subtype(4) position(4)
     * ------------------------------------------------------------------ */
    toc = (xcursor_toc_entry_t*)malloc(ntoc * sizeof(xcursor_toc_entry_t));
    if (!toc) {
        XCURSOR_DBG_ERRNO("malloc(%zu) for TOC failed", ntoc * sizeof(xcursor_toc_entry_t));
        goto cleanup;
    }

    for (uint32_t i = 0; i < ntoc; i++) {
        if (!xcursor_read_u32(fp, &toc[i].type)) {
            XCURSOR_DBG("failed to read toc[%u].type", i);
            goto cleanup;
        }
        if (!xcursor_read_u32(fp, &toc[i].subtype)) {
            XCURSOR_DBG("failed to read toc[%u].subtype", i);
            goto cleanup;
        }
        if (!xcursor_read_u32(fp, &toc[i].position)) {
            XCURSOR_DBG("failed to read toc[%u].position", i);
            goto cleanup;
        }
    }

    /* ------------------------------------------------------------------
     * Search for best-fit image chunk
     *      - Only consider chunks with type == XCURSOR_CHUNK_TYPE_IMAGE
     *      - subtype is the nominal size
     *      - Select the chunk with the smallest |subtype - target_size|
     *      - In case of a tie: prefer the smaller size (stable behavior)
     *      - If target_size == 0: take the first image chunk
     * ------------------------------------------------------------------ */
    uint32_t best_idx = UINT32_MAX;
    uint32_t best_diff = UINT32_MAX;
    uint32_t best_subtype = 0;

    for (uint32_t i = 0; i < ntoc; i++) {
        if (toc[i].type != XCURSOR_CHUNK_TYPE_IMAGE) continue;

        uint32_t sz = toc[i].subtype;
        uint32_t diff = (target_size == 0) ? 0 : xcursor_u32_abs_diff(sz, target_size);

        bool better = false;
        if (best_idx == UINT32_MAX) {
            better = true;
        } else if (diff < best_diff) {
            better = true;
        } else if (diff == best_diff && sz < best_subtype) {
            better = true;
        }

        if (better) {
            XCURSOR_DBG("new best candidate: toc[%u] size=%u diff=%u", i, sz, diff);
            best_idx = i;
            best_diff = diff;
            best_subtype = sz;
        }

        if (target_size == 0) break;
    }

    if (best_idx == UINT32_MAX) {
        XCURSOR_DBG("no image chunk found in %u TOC entries", ntoc);
        goto cleanup;
    }

    XCURSOR_DBG("selected toc[%u]: size=%u position=0x%x", best_idx, best_subtype, toc[best_idx].position);

    /* ------------------------------------------------------------------
     * Read image chunk header
     * ------------------------------------------------------------------ */
    if (fseek(fp, (long)toc[best_idx].position, SEEK_SET) != 0) {
        XCURSOR_DBG_ERRNO("fseek(chunk @ 0x%x) failed", toc[best_idx].position);
        goto cleanup;
    }

    uint32_t chunk_header_len, chunk_type, chunk_subtype, chunk_version;
    if (!xcursor_read_u32(fp, &chunk_header_len)) {
        XCURSOR_DBG("failed to read chunk_header_len");
        goto cleanup;
    }
    if (!xcursor_read_u32(fp, &chunk_type)) {
        XCURSOR_DBG("failed to read chunk_type");
        goto cleanup;
    }
    if (!xcursor_read_u32(fp, &chunk_subtype)) {
        XCURSOR_DBG("failed to read chunk_subtype");
        goto cleanup;
    }
    if (!xcursor_read_u32(fp, &chunk_version)) {
        XCURSOR_DBG("failed to read chunk_version");
        goto cleanup;
    }

    if (chunk_type != XCURSOR_CHUNK_TYPE_IMAGE) {
        XCURSOR_DBG("chunk_type mismatch: expected 0x%08x, got 0x%08x", XCURSOR_CHUNK_TYPE_IMAGE, chunk_type);
        goto cleanup;
    }
    if (chunk_subtype != toc[best_idx].subtype) {
        XCURSOR_DBG("chunk_subtype mismatch: expected %u, got %u", toc[best_idx].subtype, chunk_subtype);
        goto cleanup;
    }
    if (chunk_header_len < XCURSOR_IMAGE_HEADER_LEN) {
        XCURSOR_DBG("chunk_header_len too small: %u < %u", chunk_header_len, XCURSOR_IMAGE_HEADER_LEN);
        goto cleanup;
    }

    uint32_t width, height, xhot, yhot, delay;
    if (!xcursor_read_u32(fp, &width)) {
        XCURSOR_DBG("failed to read width");
        goto cleanup;
    }
    if (!xcursor_read_u32(fp, &height)) {
        XCURSOR_DBG("failed to read height");
        goto cleanup;
    }
    if (!xcursor_read_u32(fp, &xhot)) {
        XCURSOR_DBG("failed to read xhot");
        goto cleanup;
    }
    if (!xcursor_read_u32(fp, &yhot)) {
        XCURSOR_DBG("failed to read yhot");
        goto cleanup;
    }
    if (!xcursor_read_u32(fp, &delay)) {
        XCURSOR_DBG("failed to read delay");
        goto cleanup;
    }

    XCURSOR_DBG("image: %ux%u hot=(%u,%u) delay=%ums", width, height, xhot, yhot, delay);

    if (width == 0 || width > XCURSOR_MAX_DIM) {
        XCURSOR_DBG("width out of range: %u (max %u)", width, XCURSOR_MAX_DIM);
        goto cleanup;
    }
    if (height == 0 || height > XCURSOR_MAX_DIM) {
        XCURSOR_DBG("height out of range: %u (max %u)", height, XCURSOR_MAX_DIM);
        goto cleanup;
    }
    if (xhot > width) {
        XCURSOR_DBG("xhot %u > width %u", xhot, width);
        goto cleanup;
    }
    if (yhot > height) {
        XCURSOR_DBG("yhot %u > height %u", yhot, height);
        goto cleanup;
    }

    if (chunk_header_len > XCURSOR_IMAGE_HEADER_LEN) {
        long skip = (long)(chunk_header_len - XCURSOR_IMAGE_HEADER_LEN);
        XCURSOR_DBG("skipping %ld extra chunk-header bytes", skip);
        if (fseek(fp, skip, SEEK_CUR) != 0) {
            XCURSOR_DBG_ERRNO("fseek(skip extra chunk header, %ld) failed", skip);
            goto cleanup;
        }
    }

    /* width * height * 4 Bytes */
    size_t pixel_count = (size_t)width * (size_t)height;
    size_t pixel_bytes = pixel_count * sizeof(uint32_t);

    /* pixel_count must not overflow size_t */
    if (pixel_count != 0 && pixel_bytes / pixel_count != sizeof(uint32_t)) {
        XCURSOR_DBG("pixel_bytes overflow: width=%u height=%u", width, height);
        goto cleanup;
    }

    XCURSOR_DBG("allocating %zu bytes for %zu pixels", pixel_bytes, pixel_count);

    uint32_t* pixels = (uint32_t*)malloc(pixel_bytes);
    if (!pixels) {
        XCURSOR_DBG_ERRNO("malloc(%zu) for pixel data failed", pixel_bytes);
        goto cleanup;
    }

    if (!xcursor_read_exact(fp, pixels, pixel_bytes)) {
        XCURSOR_DBG("failed to read pixel data (%zu bytes)", pixel_bytes);
        free(pixels);
        goto cleanup;
    }

    XCURSOR_DBG("successfully loaded cursor from \"%s\"", filename);

    out_cursor->pixels = pixels;
    out_cursor->width = width;
    out_cursor->height = height;
    out_cursor->xhot = xhot;
    out_cursor->yhot = yhot;
    ok = true;

cleanup:
    free(toc);
    fclose(fp);
    return ok;
}

void xcursor_free(loaded_cursor_t* cursor) {
    if (!cursor) return;
    free(cursor->pixels);
    cursor->pixels = NULL;
    cursor->width = 0;
    cursor->height = 0;
    cursor->xhot = 0;
    cursor->yhot = 0;
}