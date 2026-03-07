#ifndef BASIC_RENDERER_H
#define BASIC_RENDERER_H
#include <vespera/devices/device_manager.h>

#include <vespera/graphics.h>
#include <vespera/terminal.h>

class GopRenderDriver final : public IRenderDriver {
   public:
    GopRenderDriver(framebuffer_t* fb, font_t* font);

    // IRenderDriver Interface
    void draw_glyph_run(const GlyphRun& run) override;
    bool fill_rect(u32 px, u32 py, u32 w, u32 h, u32 colour) override;
    bool scroll_pixels(int dy) override;
    bool blit_buffer(
        const void* pixels, u32 buffer_width, u32 buffer_height, u32 dst_x, u32 dst_y
    ) override;
    [[nodiscard]] u32 screen_width_px() const override;
    [[nodiscard]] u32 screen_height_px() const override;
    [[nodiscard]] u32 bytes_per_scanline() const override;

    [[nodiscard]] KernelDevice* get_kd() const {
        return kd_;
    }

    // Niedrig-Level Funktionen für Terminal
    void put_char(char c, u32 x, u32 y, u32 fg_color, u32 bg_color) const;
    // explizit an Position zeichnen
    void clear();  // gesamten Framebuffer löschen

   private:
    framebuffer_t* fb_;
    font_t* font_;

    KernelDevice* kd_;
};

#endif  // BASIC_RENDERER_H
