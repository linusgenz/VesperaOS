//
// Created by linus on 24.08.24.
//

#ifndef GRAHICS_H
#define GRAHICS_H
#include <vespera/types.h>

// TODO this header should be split and reworked, as it is not really fitting into the structure anymore
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
    u64 phys_base_address;
    usize   buffer_size;
    u32 width;
    u32 height;
    u32 pixels_per_scanline;
} framebuffer_t;

typedef struct {
    u32 x;
    u32 y;
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
    u32 type;      // 1 = PSF1, 2 = PSF2
    u32 width;
    u32 height;
    u32 charsize;
} font_t;

#define PSF2_MAGIC 0x864ab572

typedef struct {
    u32 magic;        // 0x864ab572
    u32 version;      // 0
    u32 headersize;   // offset of bitmaps in file
    u32 flags;        // 0 = keine Unicode Tabelle
    u32 length;       // Anzahl der Glyphen
    u32 charsize;     // Bytes pro Glyph
    u32 height;       // Pixelhöhe
    u32 width;        // Pixelbreite
} psf2_header_t;

#endif //GRAHICS_H
