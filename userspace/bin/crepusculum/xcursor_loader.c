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
#define XCURSOR_MAX_NTOC UINT32_C(0x4000) /* Sanity-Limit: 16 384 TOC-Einträge  */

typedef struct {
    uint32_t type;
    uint32_t subtype;  /* für Images: nominale Größe */
    uint32_t position; /* absoluter Byte-Offset in der Datei */
} xcursor_toc_entry_t;

static bool xcursor_read_exact(FILE* fp, void* buf, size_t n) {
    return fread(buf, 1, n, fp) == n;
}

static bool xcursor_read_u32(FILE* fp, uint32_t* out) {
    uint8_t b[4];
    if (!xcursor_read_exact(fp, b, 4)) return false;
    *out = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return true;
}

static uint32_t xcursor_u32_abs_diff(uint32_t a, uint32_t b) {
    return a >= b ? a - b : b - a;
}

bool xcursor_load_file(const char* filename, uint32_t target_size, loaded_cursor_t* out_cursor) {
    if (!filename || !out_cursor) return false;

    FILE* fp = fopen(filename, "rb");
    if (!fp) return false;

    bool ok = false;
    xcursor_toc_entry_t* toc = NULL;

    uint32_t magic, file_header_len, version, ntoc;

    if (!xcursor_read_u32(fp, &magic)) goto cleanup;
    if (!xcursor_read_u32(fp, &file_header_len)) goto cleanup;
    if (!xcursor_read_u32(fp, &version)) goto cleanup;
    if (!xcursor_read_u32(fp, &ntoc)) goto cleanup;

    if (magic != XCURSOR_MAGIC) goto cleanup;
    if (file_header_len < XCURSOR_FILE_HEADER_LEN) goto cleanup;
    if (ntoc == 0 || ntoc > XCURSOR_MAX_NTOC) goto cleanup;

    if (file_header_len > XCURSOR_FILE_HEADER_LEN) {
        long skip = (long)(file_header_len - XCURSOR_FILE_HEADER_LEN);
        if (fseek(fp, skip, SEEK_CUR) != 0) goto cleanup;
    }

    /* ------------------------------------------------------------------
     * Read toc
     *    every entry: type(4) subtype(4) position(4)
     * ------------------------------------------------------------------ */
    toc = (xcursor_toc_entry_t*)malloc(ntoc * sizeof(xcursor_toc_entry_t));
    if (!toc) goto cleanup;

    for (uint32_t i = 0; i < ntoc; i++) {
        if (!xcursor_read_u32(fp, &toc[i].type)) goto cleanup;
        if (!xcursor_read_u32(fp, &toc[i].subtype)) goto cleanup;
        if (!xcursor_read_u32(fp, &toc[i].position)) goto cleanup;
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
            best_idx = i;
            best_diff = diff;
            best_subtype = sz;
        }

        if (target_size == 0) break;
    }

    if (best_idx == UINT32_MAX) goto cleanup;

    // Read image chunk

    if (fseek(fp, (long)toc[best_idx].position, SEEK_SET) != 0) goto cleanup;

    uint32_t chunk_header_len, chunk_type, chunk_subtype, chunk_version;
    if (!xcursor_read_u32(fp, &chunk_header_len)) goto cleanup;
    if (!xcursor_read_u32(fp, &chunk_type)) goto cleanup;
    if (!xcursor_read_u32(fp, &chunk_subtype)) goto cleanup;
    if (!xcursor_read_u32(fp, &chunk_version)) goto cleanup;

    if (chunk_type != XCURSOR_CHUNK_TYPE_IMAGE) goto cleanup;
    if (chunk_subtype != toc[best_idx].subtype) goto cleanup;
    if (chunk_header_len < XCURSOR_IMAGE_HEADER_LEN) goto cleanup;

    uint32_t width, height, xhot, yhot, delay;
    if (!xcursor_read_u32(fp, &width)) goto cleanup;
    if (!xcursor_read_u32(fp, &height)) goto cleanup;
    if (!xcursor_read_u32(fp, &xhot)) goto cleanup;
    if (!xcursor_read_u32(fp, &yhot)) goto cleanup;
    if (!xcursor_read_u32(fp, &delay)) goto cleanup;

    if (width == 0 || width > XCURSOR_MAX_DIM) goto cleanup;
    if (height == 0 || height > XCURSOR_MAX_DIM) goto cleanup;
    if (xhot > width) goto cleanup;
    if (yhot > height) goto cleanup;

    if (chunk_header_len > XCURSOR_IMAGE_HEADER_LEN) {
        long skip = (long)(chunk_header_len - XCURSOR_IMAGE_HEADER_LEN);
        if (fseek(fp, skip, SEEK_CUR) != 0) goto cleanup;
    }

    /* width * height * 4 Bytes */
    size_t pixel_count = (size_t)width * (size_t)height;
    size_t pixel_bytes = pixel_count * sizeof(uint32_t);

    /* pixel_count must not overflow size_t */
    if (pixel_count != 0 && pixel_bytes / pixel_count != sizeof(uint32_t)) goto cleanup;

    uint32_t* pixels = (uint32_t*)malloc(pixel_bytes);
    if (!pixels) goto cleanup;

    if (!xcursor_read_exact(fp, pixels, pixel_bytes)) {
        free(pixels);
        goto cleanup;
    }

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