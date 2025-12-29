
#ifndef BASIC_RENDERER_H
#define BASIC_RENDERER_H
#include "../../include/graphics.h"
#include "../../kernel/graphics/terminal.h"
#include "../kernel/memory/heap.h"
#include "devices/device_manager.h"

class screen_renderer : public IRenderDriver {
public:
    screen_renderer(Framebuffer* fb, FONT* font);

    // IRenderDriver Interface
    void draw_glyph_run(const GlyphRun& run) override;
    bool fill_rect(uint32_t px, uint32_t py, uint32_t w, uint32_t h, uint32_t colour) override;
    bool scroll_pixels(int dy) override;
    [[nodiscard]] uint32_t screen_width_px() const override;
    [[nodiscard]] uint32_t screen_height_px() const override;

    // Niedrig-Level Funktionen für Terminal
    void put_char(char c, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color) const;  // explizit an Position zeichnen
    void clear();                                   // gesamten Framebuffer löschen

private:
    Framebuffer* fb;
    FONT* font;

    KernelDevice* kd;

    void scroll_down(uint32_t pixels);
};


extern Terminal* global_terminal;

#endif //BASIC_RENDERER_H