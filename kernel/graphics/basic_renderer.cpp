#include "../include/basic_renderer.h"

#include "../../include/string.h"

BasicRenderer *global_renderer;

BasicRenderer::BasicRenderer(Framebuffer *targetFramebuffer, FONT *font) {
    TargetFramebuffer = targetFramebuffer;
    PSF_Font = font;
    colour = Colour::WHITE;
    bg_colour = Colour::BLACK;
    cursor_position = {0, 0};
    cursor_visible = true;
}

void BasicRenderer::print(const char *str) {
    char *chr = (char *) str;
    while (*chr != 0) {
        if (*chr == '\n') {
            new_line();
        } else {
            put_char(*chr, cursor_position.X, cursor_position.Y);
            increment_cursorX(PSF_Font->width);
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
            increment_cursorX(PSF_Font->width);
            if (cursor_position.X + PSF_Font->width > TargetFramebuffer->width) {
                new_line();
            }
        }
    }
}


void BasicRenderer::clear() {
    const auto fb_base = reinterpret_cast<uint64_t>(TargetFramebuffer->base_address);
    const uint64_t bytes_per_scanline = TargetFramebuffer->pixels_per_scanline * 4;
    const uint64_t fb_height = TargetFramebuffer->height;

    for (int y = 0; y < fb_height; y++) {
        uint64_t pix_ptr_base = fb_base + (bytes_per_scanline * y);
        for (uint32_t *pix_ptr = reinterpret_cast<uint32_t *>(pix_ptr_base);
             pix_ptr < reinterpret_cast<uint32_t *>(pix_ptr_base + bytes_per_scanline); pix_ptr++) {
            *pix_ptr = bg_colour;
        }
    }
    scroll_manager->set_scroll_up(false);
    scroll_manager->set_scroll_down(false);
    set_cursor({0, 0});
}

void BasicRenderer::put_pixel(const uint32_t x, const uint32_t y, const Colour colour) const {
    *(uint32_t *) ((uint64_t) TargetFramebuffer->base_address + (x * 4) + (
                       y * TargetFramebuffer->pixels_per_scanline * 4)) = colour;
}

Colour BasicRenderer::get_pixel(const uint32_t x, const uint32_t y) const {
    return *(Colour *) ((uint64_t) TargetFramebuffer->base_address + (x * 4) + (
                            y * TargetFramebuffer->pixels_per_scanline * 4));
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

void BasicRenderer::clear_char() {
    if (cursor_position.X == 0) {
        cursor_position.X = TargetFramebuffer->width;
        cursor_position.Y -= PSF_Font->height;
        if (cursor_position.Y < 0) cursor_position.Y = 0;
    }

    uint32_t x_off = cursor_position.X;
    uint32_t y_off = cursor_position.Y;

    uint32_t *pixPtr = (uint32_t *) TargetFramebuffer->base_address;
    for (unsigned long y = y_off; y < y_off + PSF_Font->height; y++) {
        for (unsigned long x = x_off - PSF_Font->width; x < x_off; x++) {
            *(uint32_t *) (pixPtr + x + (y * TargetFramebuffer->pixels_per_scanline)) = bg_colour;
        }
    }

    cursor_position.X -= 8;

    if (cursor_position.X < 0) {
        cursor_position.X = TargetFramebuffer->width;
        cursor_position.Y -= PSF_Font->height;
        if (cursor_position.Y < 0) cursor_position.Y = 0;
    }
}

void BasicRenderer::put_char(char chr, uint32_t xOff, uint32_t yOff) {
    if (chr == '\0') return;
    auto *pix_ptr = (uint32_t *) TargetFramebuffer->base_address;
    const char *glyph = (char *) PSF_Font->glyphBuffer + (chr * PSF_Font->charsize);
    for (uint32_t y = 0; y < PSF_Font->height; y++) {
        uint32_t row = 0;
        for (uint32_t bx = 0; bx < (PSF_Font->width + 7) / 8; bx++) {
            uint8_t byte = glyph[y * ((PSF_Font->width + 7) / 8) + bx];
            for (uint32_t bit = 0; bit < 8; bit++) {
                uint32_t x = bx * 8 + bit;
                if (x >= PSF_Font->width) break;
                uint32_t color_to_draw = (byte & (0x80 >> bit)) ? colour : bg_colour;
                *(pix_ptr + (xOff + x) + (yOff + y) * TargetFramebuffer->pixels_per_scanline) = color_to_draw;
            }
        }
    }
}

void BasicRenderer::put_char(char chr) {
    clear_cursor(cursor_position.X, cursor_position.Y);
    put_char(chr, cursor_position.X, cursor_position.Y);
    cursor_position.X += PSF_Font->width;
    if (cursor_position.X + PSF_Font->width > TargetFramebuffer->width) {
        new_line();
    }
    // draw_cursor();
}

void BasicRenderer::draw_cursor() const {
    uint32_t *pix_ptr = (uint32_t *) TargetFramebuffer->base_address;

    uint64_t max_y = min(cursor_position.Y + PSF_Font->height, TargetFramebuffer->height);
    uint64_t max_x = min(cursor_position.X + PSF_Font->width, TargetFramebuffer->width);

    for (uint64_t y = cursor_position.Y; y < max_y; y++) {
        for (uint64_t x = cursor_position.X; x < max_x; x++) {
            *(uint32_t *) (pix_ptr + x + (y * TargetFramebuffer->pixels_per_scanline)) = Colour::WHITE;
        }
    }
}

void BasicRenderer::clear_cursor(uint64_t x_pos, uint64_t y_pos) const {
    uint32_t *pix_ptr = (uint32_t *) TargetFramebuffer->base_address;

    uint64_t max_y = min(y_pos + PSF_Font->height, TargetFramebuffer->height);
    uint64_t max_x = min(x_pos + PSF_Font->width, TargetFramebuffer->width);

    for (uint64_t y = y_pos; y < max_y; y++) {
        for (uint64_t x = x_pos; x < max_x; x++) {
            *(uint32_t *) (pix_ptr + x + (y * TargetFramebuffer->pixels_per_scanline)) = Colour::BLACK;
        }
    }
}

void BasicRenderer::new_line() {
    cursor_position.X = 0;
    cursor_position.Y += PSF_Font->height;

    if (cursor_position.Y + PSF_Font->height >= TargetFramebuffer->height) {
        scroll_manager->setup_new_line();
        cursor_position.Y -= PSF_Font->height;
    }
}

Point BasicRenderer::get_cursor_pos() const {
    return cursor_position;
}
