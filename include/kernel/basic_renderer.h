
#ifndef BASIC_RENDERER_H
#define BASIC_RENDERER_H
#include "../../include/graphics.h"
#include "../../kernel/graphics/IScreenRenderer.h"
#include "../kernel/memory/heap.h"

class screen_renderer{
    public:
    screen_renderer(Framebuffer* targetFramebuffer, FONT* font);

    Framebuffer* TargetFramebuffer;
    FONT* PSF_Font;
    void print(const char* str);

    void print(const char *str, size_t length);

    [[nodiscard]] uint32_t get_pixel(uint32_t x, uint32_t y) const;
    void put_char(char chr, uint32_t xOff, uint32_t yOff) const;
    void put_char(char chr);
    inline void set_cursorX(int32_t x);
    inline void set_cursorY(int32_t y);
    inline void set_cursor(Point pt);
    void decrement_cursorX(int32_t x);
    void decrement_cursorY(int32_t y);
    inline void increment_cursorX(uint32_t x);
    inline void increment_cursorY(uint32_t y);
    inline void set_colour(uint32_t new_colour);
    [[nodiscard]] inline uint32_t get_colour() const;
    [[nodiscard]] inline uint32_t get_bg_colour() const;
    inline void set_bg_colour(uint32_t new_colour);
    void new_line();
    void clear();
    void put_pixel(uint32_t x, uint32_t y, uint32_t colour);
    void clear_char();
    void draw_overlay_mouse_cursor(const uint8_t* mouse_cursor, Point position, uint32_t colour);
    void draw_cursor() const;
    void clear_cursor(uint64_t x_pos, uint64_t y_pos) const;
    void scroll_down();
    void clear_mouse_cursor(const uint8_t* mouse_cursor, Point position) const;
    uint32_t mouse_cursor_buffer[16 * 16]{};
    uint32_t mouse_cursor_buffer_after[16 * 16]{};
    bool cursor_visible;
    [[nodiscard]] Point get_cursor_pos() const;
    private:
    Point cursor_position{};
    uint32_t colour;
    uint32_t bg_colour;
    bool mouse_drawn{};
};

inline void screen_renderer::set_cursorX(int32_t x) {
    cursor_position.X = x;
}

inline void screen_renderer::set_cursorY(int32_t y) {
    cursor_position.Y = y;
}

inline void screen_renderer::set_cursor(Point pt) {
    cursor_position.X = pt.X;
    cursor_position.Y = pt.Y;
}

inline void screen_renderer::increment_cursorX(uint32_t x) {
    cursor_position.X += x;
}

inline void screen_renderer::increment_cursorY(uint32_t y) {
    cursor_position.Y += y;
}

inline void screen_renderer::decrement_cursorX(int32_t x) {
    cursor_position.X -= x;
}

inline void screen_renderer::decrement_cursorY(int32_t y) {
    cursor_position.Y -= y;
}

inline void screen_renderer::set_colour(uint32_t new_colour) {
    colour = new_colour;
}

inline uint32_t screen_renderer::get_colour() const {
    return colour;
}

inline uint32_t screen_renderer::get_bg_colour() const {
    return bg_colour;
}

inline void screen_renderer::set_bg_colour(uint32_t new_colour) {
    bg_colour = new_colour;
}

extern IScreenRenderer* global_renderer;

#endif //BASIC_RENDERER_H