#ifndef BASIC_RENDERER_H
#define BASIC_RENDERER_H
#include "../../include/graphics.h"
#include "../../include/kernel/terminal.h"
#include "../memory/heap.h"
#include <kernel/devices/device_manager.h>

class gop_render_driver : public IRenderDriver
{
public:
    gop_render_driver(Framebuffer* fb, FONT* font);

    // IRenderDriver Interface
    void draw_glyph_run(const GlyphRun& run) override;
    bool fill_rect(uint32_t px, uint32_t py, uint32_t w, uint32_t h, uint32_t colour) override;
    bool scroll_pixels(int dy) override;
    bool blit_buffer(const void* pixels, uint32_t buffer_width, uint32_t buffer_height, uint32_t dst_x,
                     uint32_t dst_y) override;
    [[nodiscard]] uint32_t screen_width_px() const override;
    [[nodiscard]] uint32_t screen_height_px() const override;
    [[nodiscard]] uint32_t bytes_per_scanline() const override;

    [[nodiscard]] KernelDevice* get_kd() const
    {
        return kd;
    }

    // Niedrig-Level Funktionen für Terminal
    void put_char(char c, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color) const;
    // explizit an Position zeichnen
    void clear(); // gesamten Framebuffer löschen

private:
    Framebuffer* fb;
    FONT* font;

    KernelDevice* kd;

    void scroll_down(uint32_t pixels);
};

#endif //BASIC_RENDERER_H
