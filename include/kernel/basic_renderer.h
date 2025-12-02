
#ifndef BASIC_RENDERER_H
#define BASIC_RENDERER_H
#include "../../include/graphics.h"
#include "../kernel/memory/heap.h"

class BasicRenderer{
    public:
    BasicRenderer(Framebuffer* targetFramebuffer, FONT* font);

    Framebuffer* TargetFramebuffer;
    FONT* PSF_Font;
    void print(const char* str);

    void print(const char *str, size_t length);

    [[nodiscard]] Colour get_pixel(uint32_t x, uint32_t y) const;
    void put_char(char chr, uint32_t xOff, uint32_t yOff) const;
    void put_char(char chr);
    inline void set_cursorX(int32_t x);
    inline void set_cursorY(int32_t y);
    inline void set_cursor(Point pt);
    void decrement_cursorX(int32_t x);
    void decrement_cursorY(int32_t y);
    inline void increment_cursorX(uint32_t x);
    inline void increment_cursorY(uint32_t y);
    inline void set_colour(Colour colour);
    [[nodiscard]] inline Colour get_colour() const;
    [[nodiscard]] inline Colour get_bg_colour() const;
    inline void set_bg_colour(Colour colour);
    void new_line();
    void clear();
    void clear_char();
    void draw_overlay_mouse_cursor(const uint8_t* mouse_cursor, Point position, Colour colour);
    void draw_cursor() const;
    void clear_cursor(uint64_t x_pos, uint64_t y_pos) const;
    void clear_mouse_cursor(const uint8_t* mouse_cursor, Point position) const;
    Colour mouse_cursor_buffer[16 * 16]{};
    Colour mouse_cursor_buffer_after[16 * 16]{};
    bool cursor_visible;
    [[nodiscard]] Point get_cursor_pos() const;
    private:
    Point cursor_position{};
    Colour colour;
    Colour bg_colour;
    bool mouse_drawn{};
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

inline void BasicRenderer::increment_cursorX(uint32_t x) {
    cursor_position.X += x;
}

inline void BasicRenderer::increment_cursorY(uint32_t y) {
    cursor_position.Y += y;
}

inline void BasicRenderer::decrement_cursorX(int32_t x) {
    cursor_position.X -= x;
}

inline void BasicRenderer::decrement_cursorY(int32_t y) {
    cursor_position.Y -= y;
}

inline void BasicRenderer::set_colour(Colour new_colour) {
    colour = new_colour;
}

inline Colour BasicRenderer::get_colour() const {
    return colour;
}

inline Colour BasicRenderer::get_bg_colour() const {
    return bg_colour;
}

inline void BasicRenderer::set_bg_colour(Colour new_colour) {
    bg_colour = new_colour;
}

extern BasicRenderer* global_renderer;

#endif //BASIC_RENDERER_H