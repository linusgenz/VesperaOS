#include "../include/basic_renderer.h"

#include "../../include/string.h"

BasicRenderer *global_renderer;

BasicRenderer::BasicRenderer(Framebuffer *targetFramebuffer, PSF1_FONT *psf1_Font) {
    TargetFramebuffer = targetFramebuffer;
    PSF1_Font = psf1_Font;
    colour = Colour::WHITE;
    clear_colour = Colour::BLACK;
    cursor_position = {0, 0};
    cursor_visible = true;

    draw_buffer = nullptr;
}

BasicRenderer::~BasicRenderer() {
    // Clean up allocated buffers
    if (draw_buffer) free(draw_buffer);
}

void BasicRenderer::setup_buffer() {
    buffer_size = TargetFramebuffer->width * TargetFramebuffer->height * sizeof(uint32_t);
    draw_buffer = (uint32_t *) malloc(buffer_size);

    if (!draw_buffer) {
        draw_buffer = nullptr;
        return;
    }

    // Clear initial buffer
    uint32_t pixel_count = TargetFramebuffer->width * TargetFramebuffer->height;
    for (uint32_t i = 0; i < pixel_count; i++) {
        draw_buffer[i] = clear_colour;
    }

    present();
}


uint32_t *BasicRenderer::get_draw_buffer() const {
    return draw_buffer;
}

void BasicRenderer::present() {
    if (!draw_buffer) return;

    uint32_t *dst = (uint32_t*)TargetFramebuffer->base_address;
    uint32_t *src = draw_buffer;

    size_t width = TargetFramebuffer->width;
    size_t stride = TargetFramebuffer->pixels_per_scanline;

    for (size_t y = 0; y < TargetFramebuffer->height; y++) {
        fast_memcpy(dst + y * stride, src + y * width, width * sizeof(uint32_t));
    }
}

void BasicRenderer::fast_memcpy(void *dst, const void *src, size_t size) {
    uint64_t *dst64 = (uint64_t *) dst;
    const uint64_t *src64 = (const uint64_t *) src;
    size_t size64 = size / 8;

    for (size_t i = 0; i < size64; i++) {
        dst64[i] = src64[i];
    }

    uint8_t *dst8 = (uint8_t *) dst + (size64 * 8);
    const uint8_t *src8 = (const uint8_t *) src + (size64 * 8);
    size_t remaining = size % 8;

    for (size_t i = 0; i < remaining; i++) {
        dst8[i] = src8[i];
    }
}

void BasicRenderer::clear_screen(Colour c) {
    uint32_t pixel_count = TargetFramebuffer->width * TargetFramebuffer->height;
    for (uint32_t i = 0; i < pixel_count; i++) {
        draw_buffer[i] = c;
    }
    cursor_position = {0, 0};
  //  scroll_manager->set_scroll_up(false);
  //  scroll_manager->set_scroll_down(false);

    present();
}


void BasicRenderer::put_pixel(const uint32_t x, const uint32_t y, const Colour colour) const {
    if (x >= TargetFramebuffer->width || y >= TargetFramebuffer->height) return;

    uint32_t *buffer = get_draw_buffer();
    buffer[x + y * TargetFramebuffer->width] = colour;
}

Colour BasicRenderer::get_pixel(const uint32_t x, const uint32_t y) const {
    if (x >= TargetFramebuffer->width || y >= TargetFramebuffer->height) return Colour::BLACK;

    uint32_t *buffer = get_draw_buffer();
    return (Colour) buffer[x + y * TargetFramebuffer->width];
}

void BasicRenderer::clear_char() {
    if (cursor_position.X == 0) {
        cursor_position.X = TargetFramebuffer->width;
        cursor_position.Y -= 16;
        if (cursor_position.Y < 0) cursor_position.Y = 0;
    }

    uint32_t x_off = cursor_position.X;
    uint32_t y_off = cursor_position.Y;

    uint32_t *buffer = get_draw_buffer();

    for (unsigned long y = y_off; y < y_off + 16 && y < TargetFramebuffer->height; y++) {
        for (unsigned long x = x_off - 8; x < x_off && x < TargetFramebuffer->width; x++) {
            buffer[x + (y * TargetFramebuffer->width)] = clear_colour;
        }
    }

    cursor_position.X -= 8;

    if (cursor_position.X < 0) {
        cursor_position.X = TargetFramebuffer->width;
        cursor_position.Y -= 16;
        if (cursor_position.Y < 0) cursor_position.Y = 0;
    }

    present();
}

void BasicRenderer::put_char(char chr, uint32_t xOff, uint32_t yOff) {
    if (chr == '\0') return;

    uint32_t *buffer = get_draw_buffer();
    char *font_ptr = (char *) PSF1_Font->glyphBuffer + (chr * PSF1_Font->psf1_header->charsize);

    for (unsigned long y = yOff; y < yOff + 16 && y < TargetFramebuffer->height; y++) {
        for (unsigned long x = xOff; x < xOff + 8 && x < TargetFramebuffer->width; x++) {
            if ((*font_ptr & (0b10000000 >> (x - xOff))) > 0) {
                buffer[x + y * TargetFramebuffer->width] = colour;
            }
        }
        font_ptr++;
    }
}

void BasicRenderer::put_char(char chr) {
    clear_cursor(cursor_position.X, cursor_position.Y);
    put_char(chr, cursor_position.X, cursor_position.Y);
    cursor_position.X += 8;
    if (cursor_position.X + 8 > TargetFramebuffer->width) {
        new_line();
    }
    // draw_cursor(); // Wenn du den Cursor sehen willst
}

void BasicRenderer::draw_cursor() const {
    if (!cursor_visible) return;

    uint32_t *buffer = get_draw_buffer();
    uint64_t max_y = min(cursor_position.Y + 16, TargetFramebuffer->height);
    uint64_t max_x = min(cursor_position.X + 8, TargetFramebuffer->width);

    for (uint64_t y = cursor_position.Y; y < max_y; y++) {
        for (uint64_t x = cursor_position.X; x < max_x; x++) {
            buffer[x + y * TargetFramebuffer->width] = Colour::WHITE;
        }
    }
}

void BasicRenderer::clear_cursor(uint64_t x_pos, uint64_t y_pos) const {
    uint32_t *buffer = get_draw_buffer();
    uint64_t max_y = min(y_pos + 16, TargetFramebuffer->height);
    uint64_t max_x = min(x_pos + 8, TargetFramebuffer->width);

    for (uint64_t y = y_pos; y < max_y; y++) {
        for (uint64_t x = x_pos; x < max_x; x++) {
            buffer[x + y * TargetFramebuffer->width] = clear_colour;
        }
    }
}

void BasicRenderer::copy_line(uint32_t src_char_line, uint32_t dst_char_line) {
    uint32_t* buffer = get_draw_buffer(); // kompakter Buffer
    size_t width = TargetFramebuffer->width;

    uint32_t* src = buffer + src_char_line * 16 * width;
    uint32_t* dst = buffer + dst_char_line * 16 * width;

    size_t line_bytes = width * sizeof(uint32_t);
    for (int i = 0; i < 16; i++) {
        fast_memcpy(dst + i * width, src + i * width, line_bytes);
    }
}


void BasicRenderer::clear_line(uint32_t char_line) {
    uint32_t* buffer = get_draw_buffer();
    size_t width = TargetFramebuffer->width;

    uint32_t* line_start = buffer + char_line * 16 * width;
    const uint64_t clear_value64 = ((uint64_t)clear_colour << 32) | clear_colour;

    for (int y = 0; y < 16; y++) {
        uint64_t* line64 = (uint64_t*)(line_start + y * width);
        size_t chunks64 = width / 2;
        for (size_t i = 0; i < chunks64; i++) {
            line64[i] = clear_value64;
        }
        if (width % 2) {
            line_start[y * width + width - 1] = clear_colour;
        }
    }
}


void BasicRenderer::print(const char *str) {
    char *chr = (char *) str;
    while (*chr != 0) {
        if (*chr == '\n') {
            new_line();
        } else {
            put_char(*chr, cursor_position.X, cursor_position.Y);
            increment_cursorX(8);
            if (cursor_position.X + 8 > TargetFramebuffer->width) {
                new_line();
            }
        }
        chr++;
    }
}

void BasicRenderer::print(const char *str, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (str[i] == '\n') {
            new_line();
        } else {
            put_char(str[i], cursor_position.X, cursor_position.Y);
            increment_cursorX(8);
            if (cursor_position.X + 8 > TargetFramebuffer->width) {
                new_line();
            }
        }
    }
}

inline void fast_memmove(uint32_t* dest, const uint32_t* src, size_t bytes) {
    if (dest == src || bytes == 0) return;

    size_t pixels = bytes / sizeof(uint32_t);

    if (dest < src) {
        size_t chunks64 = pixels / 2;
        uint64_t* d64 = reinterpret_cast<uint64_t*>(dest);
        const uint64_t* s64 = reinterpret_cast<const uint64_t*>(src);
        for (size_t i = 0; i < chunks64; i++) {
            d64[i] = s64[i];
        }
        if (pixels % 2) {
            dest[pixels - 1] = src[pixels - 1];
        }
    } else {
        size_t chunks64 = pixels / 2;
        uint64_t* d64 = reinterpret_cast<uint64_t*>(dest + (pixels - pixels % 2 - 2));
        const uint64_t* s64 = reinterpret_cast<const uint64_t*>(src + (pixels - pixels % 2 - 2));
        for (size_t i = 0; i < chunks64; i++) {
            d64[-(int)i] = s64[-(int)i];
        }
        if (pixels % 2) {
            dest[0] = src[0];
        }
    }
}

void BasicRenderer::new_line() {
    cursor_position.X = 0;
    cursor_position.Y += 16;

    if (cursor_position.Y + 16 >= TargetFramebuffer->height) {
    //    scroll_manager->setup_new_line();
        uint32_t lines_per_screen = TargetFramebuffer->height / 16;

        uint32_t* buffer = get_draw_buffer();
        size_t pixels_per_line = 16 * TargetFramebuffer->width;
        size_t bytes_to_move = (lines_per_screen - 1) * pixels_per_line * sizeof(uint32_t);

        fast_memmove(buffer, buffer + pixels_per_line, bytes_to_move);

        cursor_position.Y -= 16;

        clear_line(lines_per_screen - 1);
    }
}

Point BasicRenderer::get_cursor_pos() const {
    return cursor_position;
}


void BasicRenderer::clear_mouse_cursor(const uint8_t *mouse_cursor, const Point position) const {
    if (!mouse_drawn) return;

    int32_t x_max = 16;
    int32_t y_max = 16;
    int32_t diffrence_x = TargetFramebuffer->width - position.X;
    int32_t diffrence_y = TargetFramebuffer->height - position.Y;

    if (diffrence_x < 16) x_max = diffrence_x;
    if (diffrence_y < 16) y_max = diffrence_y;

    for (int32_t y = 0; y < y_max; y++) {
        for (int32_t x = 0; x < x_max; x++) {
            int32_t bit = y * 16 + x;
            int32_t byte = bit / 8;
            if ((mouse_cursor[byte] & (0b10000000 >> (x % 8)))) {
                if (get_pixel(position.X + x, position.Y + y) == mouse_cursor_buffer_after[x + y * 16]) {
                    put_pixel(position.X + x, position.Y + y, mouse_cursor_buffer[x + y * 16]);
                }
            }
        }
    }
}

void BasicRenderer::draw_overlay_mouse_cursor(const uint8_t *mouse_cursor, const Point position, const Colour colour) {
    int32_t x_max = 16;
    int32_t y_max = 16;
    int32_t diffrence_x = TargetFramebuffer->width - position.X;
    int32_t diffrence_y = TargetFramebuffer->height - position.Y;

    if (diffrence_x < 16) x_max = diffrence_x;
    if (diffrence_y < 16) y_max = diffrence_y;

    for (int32_t y = 0; y < y_max; y++) {
        for (int32_t x = 0; x < x_max; x++) {
            int32_t bit = y * 16 + x;
            int32_t byte = bit / 8;
            if ((mouse_cursor[byte] & (0b10000000 >> (x % 8)))) {
                mouse_cursor_buffer[x + y * 16] = get_pixel(position.X + x, position.Y + y);
                put_pixel(position.X + x, position.Y + y, colour);
                mouse_cursor_buffer_after[x + y * 16] = get_pixel(position.X + x, position.Y + y);
            }
        }
    }

    mouse_drawn = true;
}
