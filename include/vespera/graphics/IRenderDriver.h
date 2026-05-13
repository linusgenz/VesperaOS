#ifndef VESPERAOS_GRAPHICS_IRENDERDRIVER_H
#define VESPERAOS_GRAPHICS_IRENDERDRIVER_H

#include <vespera/types.h>

class IRenderDriver {
   public:
    virtual ~IRenderDriver() = default;

    virtual bool fill_rect(u32 px, u32 py, u32 w, u32 h, u32 colour) = 0;

    virtual bool blit_buffer(
        const void* pixels, u32 buffer_width, u32 buffer_height, u32 dst_x, u32 dst_y
    ) = 0;

    virtual bool scroll_pixels(int dy) = 0;

    [[nodiscard]] virtual u32 screen_width_px() const = 0;
    [[nodiscard]] virtual u32 screen_height_px() const = 0;
    [[nodiscard]] virtual u32 bytes_per_scanline() const = 0;
};

#endif  // VESPERAOS_GRAPHICS_IRENDERDRIVER_H
