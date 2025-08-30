
#ifndef BASIC_RENDERER_H
#define BASIC_RENDERER_H
#include "../../include/graphics.h"
#include "../../include/utils.h"
#include "../include/memory.h"
#include "../memory/heap.h"
#include "ScrollManager.h"

class BasicRenderer{
    public:
    BasicRenderer(Framebuffer* targetFramebuffer, PSF1_FONT* psf1_Font);

    ~BasicRenderer();

    void setup_buffer();

    Framebuffer* TargetFramebuffer;
    PSF1_FONT* PSF1_Font;

    void fast_memcpy(void* dst, const void* src, size_t size);
    uint32_t* get_draw_buffer() const;
    void present();

    void print(const char* str);
    void print(const char *str, size_t length);

    void put_pixel(uint32_t x, uint32_t y, Colour colour) const;
    Colour get_pixel(uint32_t x, uint32_t y) const;
    void put_char(char chr, uint32_t xOff, uint32_t yOff);
    void put_char(char chr);
    inline void set_cursorX(int32_t x);
    inline void set_cursorY(int32_t y);
    inline void set_cursor(Point pt);
    inline void set_colour(Colour colour);
    inline Colour get_colour() const;
    Colour get_clear_color() const;
    inline void set_clear_color(Colour colour);
    inline void increment_cursorX(int32_t x);
    inline void increment_cursorY(int32_t y);
    void new_line();
    void clear_screen(Colour colour = Colour::BLACK);
    void clear_char();
    void draw_overlay_mouse_cursor(const uint8_t* mouse_cursor, Point position, Colour colour);
    void draw_cursor() const;
    void clear_cursor(uint64_t x_pos, uint64_t y_pos) const;

    void copy_line(uint32_t src_line, uint32_t dst_line);

    void clear_line(uint32_t line);

    void clear_mouse_cursor(const uint8_t* mouse_cursor, Point position) const;
    Colour mouse_cursor_buffer[16 * 16];
    Colour mouse_cursor_buffer_after[16 * 16];
    bool cursor_visible;
    Point get_cursor_pos() const;
    private:
    Point cursor_position;
    Colour colour;
    Colour clear_colour;
    bool mouse_drawn;
    uint32_t* draw_buffer;
    size_t buffer_size;
};

inline void BasicRenderer::set_cursorX(int32_t x) {
    cursor_position.X = x;
}

inline void BasicRenderer::set_cursorY(int32_t y) {
    cursor_position.Y = y;
}

inline void BasicRenderer::set_cursor(Point pt) {
    cursor_position.X = pt.X;
    cursor_position.Y = pt.Y;
}

inline void BasicRenderer::increment_cursorX(int32_t x) {
    cursor_position.X += x;
}

inline void BasicRenderer::increment_cursorY(int32_t y) {
    cursor_position.Y += y;
}

inline void BasicRenderer::set_colour(Colour new_colour) {
    colour = new_colour;
}

inline Colour BasicRenderer::get_colour() const {
    return colour;
}

inline Colour BasicRenderer::get_clear_color() const {
    return clear_colour;
}

inline void BasicRenderer::set_clear_color(Colour new_colour) {
    clear_colour = new_colour;
}

extern BasicRenderer* global_renderer;

#endif //BASIC_RENDERER_H