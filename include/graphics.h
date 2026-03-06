//
// Created by linus on 24.08.24.
//

#ifndef GRAHICS_H
#define GRAHICS_H
#include <stdint.h>
#include <stddef.h>

typedef enum {
    BLACK   = 0x00000000,
    WHITE   = 0x00FFFFFF,
    RED     = 0x00FF0000,
    GREEN   = 0x0000FF00,
    BLUE    = 0x000000FF,
    YELLOW  = 0x00FFFF00,
    CYAN    = 0x0000FFFF,
    MAGENTA = 0x00FF00FF,
    ORANGE  = 0x0000A5FF,
    GRAY    = 0x00808080,
    BG_COLOUR = 0x00061220,
} colour_t;

typedef struct {
    void*    base_address;
    uint64_t phys_base_address;
    size_t   buffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t pixels_per_scanline;
} framebuffer_t;

typedef struct {
    uint32_t x;
    uint32_t y;
} point_t;

#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04

typedef struct {
    unsigned char magic[2];
    unsigned char mode;
    unsigned char charsize;
} psf1_header_t;

typedef struct {
    void* header;       // PSF1_HEADER* or PSF2_HEADER*
    void* glyph_buffer;
    uint32_t type;      // 1 = PSF1, 2 = PSF2
    uint32_t width;
    uint32_t height;
    uint32_t charsize;
} font_t;

#define PSF2_MAGIC 0x864ab572

typedef struct {
    uint32_t magic;        // 0x864ab572
    uint32_t version;      // 0
    uint32_t headersize;   // offset of bitmaps in file
    uint32_t flags;        // 0 = keine Unicode Tabelle
    uint32_t length;       // Anzahl der Glyphen
    uint32_t charsize;     // Bytes pro Glyph
    uint32_t height;       // Pixelhöhe
    uint32_t width;        // Pixelbreite
} psf2_header_t;

#endif //GRAHICS_H
