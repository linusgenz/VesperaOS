#ifndef VESPERAOS_GRAPHICS_IRENDERDRIVER_H
#define VESPERAOS_GRAPHICS_IRENDERDRIVER_H

#include <vespera/types.h>

class IRenderDriver {
   public:
    virtual ~IRenderDriver() = default;

    virtual bool fill_rect(u32 px, u32 py, u32 w, u32 h, u32 colour) = 0;

    /**
     * @brief Blit a sub-rectangle of a strided pixel buffer to the screen.
     *
     * @param pixels     Pointer to the top-left of the full source buffer
     * @param src_stride Width of the full source buffer in pixels
     * @param src_x      Left edge of the source rectangle
     * @param src_y      Top edge of the source rectangle
     * @param w          Width of the rectangle in pixels
     * @param h          Height of the rectangle in pixels
     * @param dst_x      X destination on screen
     * @param dst_y      Y destination on screen
     */
    [[nodiscard]] virtual bool blit_region(
        const u32* pixels, u32 src_stride, u32 src_x, u32 src_y, u32 w, u32 h, u32 dst_x, u32 dst_y
    ) = 0;

    virtual void present() {};

    [[nodiscard]] virtual bool has_hw_cursor() const {
        return false;
    }

    [[nodiscard]] virtual bool set_cursor_image(
        const u32* /*pixels*/, u32 /*width*/, u32 /*height*/, i32 /*hotspot_x*/, i32 /*hotspot_y*/
    ) {
        return false;
    }

    virtual void move_cursor(i32 /*x*/, i32 /*y*/) {}
    virtual void set_cursor_visible(bool /*visible*/) {}

    [[nodiscard]] virtual u32 screen_width_px() const = 0;
    [[nodiscard]] virtual u32 screen_height_px() const = 0;
    [[nodiscard]] virtual u32 bytes_per_scanline() const = 0;
};

#endif  // VESPERAOS_GRAPHICS_IRENDERDRIVER_H
