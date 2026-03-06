#include "gop_render_driver.h"

#include "../../filesystem/devfs/devfs.h"
#include "../../include/kernel/terminal.h"
#include "kernel/devices/device_manager.h"
#include "kernel/memory.h"

GopRenderDriver::GopRenderDriver(framebuffer_t* fb, font_t* font)
    : fb_(fb)
    , font_(font) {
    char name[10];
    DeviceManager::alloc_unique_device_name("uefi_gop", name, sizeof(name));
    kd_ = DeviceManager::register_gpu_device(
        this, name, DeviceClass::Graphics, BusType::VIRTUAL, ControllerType::UefiGop, nullptr
    );
    DevFs::register_device(kd_);
}

void GopRenderDriver::draw_glyph_run(const GlyphRun& run) {
    for (uint32_t i = 0; i < run.length; i++) {
        put_char(run.text[i], run.px + i * font_->width, run.py, run.fg, run.bg);
    }
}

bool GopRenderDriver::fill_rect(uint32_t px, uint32_t py, uint32_t w, uint32_t h, uint32_t colour) {
    if (px + w > fb_->width || py + h > fb_->height) return false;

    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint32_t* pix = static_cast<uint32_t*>(fb_->base_address) + (px + x) + (py + y) * fb_->pixels_per_scanline;
            *pix = colour;
        }
    }

    return true;
}

void GopRenderDriver::clear() {
    fill_rect(0, 0, fb_->width, fb_->height, 0x00000000);
}

/*
void gop_render_driver::clear_mouse_cursor(const uint8_t* mouse_cursor, const Point position) const
{
      if (!mouse_drawn) return;

      int32_t x_max = 16;
      int32_t y_max = 16;
      int32_t diffrence_x = TargetFramebuffer->width - position.X;
      int32_t diffrence_y = TargetFramebuffer->height - position.Y;

      if (diffrence_x < 16) x_max = diffrence_x;
      if (diffrence_y < 16) y_max = diffrence_y;

      for (int32_t y = 0; y < y_max; y++)
      {
          for (int32_t x = 0; x < x_max; x++)
          {
              int32_t bit = y * 16 + x;
              int32_t byte = bit / 8;
              if ((mouse_cursor[byte] & (0b10000000 >> (x % 8))))
              {
                  if (get_pixel(position.X + x, position.Y + y) == mouse_cursor_buffer_after[x + y * 16])
                  {
                      put_pixel(position.X + x, position.Y + y, mouse_cursor_buffer[x + y * 16]);
                  }
              }
          }
      }
}


void gop_render_driver::draw_overlay_mouse_cursor(const uint8_t* mouse_cursor, const Point position, const uint32_t
colour)
{
      int32_t x_max = 16;
      int32_t y_max = 16;
      int32_t diffrence_x = TargetFramebuffer->width - position.X;
      int32_t diffrence_y = TargetFramebuffer->height - position.Y;

      if (diffrence_x < 16) x_max = diffrence_x;
      if (diffrence_y < 16) y_max = diffrence_y;

      for (int32_t y = 0; y < y_max; y++)
      {
          for (int32_t x = 0; x < x_max; x++)
          {
              int32_t bit = y * 16 + x;
              int32_t byte = bit / 8;
              if ((mouse_cursor[byte] & (0b10000000 >> (x % 8))))
              {
                  mouse_cursor_buffer[x + y * 16] = get_pixel(position.X + x, position.Y + y);
                  put_pixel(position.X + x, position.Y + y, colour);
                  mouse_cursor_buffer_after[x + y * 16] = get_pixel(position.X + x, position.Y + y);
              }
          }
      }

      mouse_drawn = true;

}*/

void GopRenderDriver::put_char(char c, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color) const {
    if (!c) return;

    auto pix_ptr = static_cast<uint32_t*>(fb_->base_address);
    const char* glyph = static_cast<char*>(font_->glyph_buffer) + (c * font_->charsize);

    for (uint32_t row = 0; row < font_->height; row++) {
        for (uint32_t bx = 0; bx < (font_->width + 7) / 8; bx++) {
            uint8_t byte = glyph[row * ((font_->width + 7) / 8) + bx];
            for (uint32_t bit = 0; bit < 8; bit++) {
                uint32_t xpix = bx * 8 + bit;
                if (xpix >= font_->width) break;

                uint32_t color_to_draw = (byte & (0x80 >> bit)) ? fg_color : bg_color;
                *(pix_ptr + (x + xpix) + (y + row) * fb_->pixels_per_scanline) = color_to_draw;
            }
        }
    }
}

/*
void gop_render_driver::draw_cursor() const
{
    auto* pix_ptr = static_cast<uint32_t*>(TargetFramebuffer->base_address);

    uint64_t max_y = min(cursor_position.Y + PSF_Font->height, TargetFramebuffer->height);
    uint64_t max_x = min(cursor_position.X + PSF_Font->width, TargetFramebuffer->width);

    for (uint64_t y = cursor_position.Y; y < max_y; y++)
    {
        for (uint64_t x = cursor_position.X; x < max_x; x++)
        {
            *(pix_ptr + x + y * TargetFramebuffer->pixels_per_scanline) = WHITE;
        }
    }
}

void gop_render_driver::clear_cursor(uint64_t x_pos, uint64_t y_pos) const
{
    auto* pix_ptr = static_cast<uint32_t*>(TargetFramebuffer->base_address);

    uint64_t max_y = min(y_pos + PSF_Font->height, TargetFramebuffer->height);
    uint64_t max_x = min(x_pos + PSF_Font->width, TargetFramebuffer->width);

    for (uint64_t y = y_pos; y < max_y; y++)
    {
        for (uint64_t x = x_pos; x < max_x; x++)
        {
            *(pix_ptr + x + y * TargetFramebuffer->pixels_per_scanline) = BLACK;
        }
    }
}*/

bool GopRenderDriver::blit_buffer(
    const void* pixels, uint32_t buffer_width, uint32_t buffer_height, uint32_t dst_x, uint32_t dst_y
) {
    if (!pixels) return false;

    uint32_t max_w = buffer_width;
    uint32_t max_h = buffer_height;

    if (dst_x >= fb_->width || dst_y >= fb_->height) return false;

    if (dst_x + buffer_width > fb_->width) max_w = fb_->width - dst_x;
    if (dst_y + buffer_height > fb_->height) max_h = fb_->height - dst_y;

    const auto* src = static_cast<const uint32_t*>(pixels);
    auto* dst = static_cast<uint32_t*>(fb_->base_address);

    for (uint32_t y = 0; y < max_h; y++) {
        uint32_t* dst_row = dst + (dst_y + y) * fb_->pixels_per_scanline + dst_x;
        const uint32_t* src_row = src + y * buffer_width;

        memcpy(dst_row, src_row, max_w * sizeof(uint32_t));
    }

    return true;
}

bool GopRenderDriver::scroll_pixels(int dy) {
    if (dy <= 0 || static_cast<uint32_t>(dy) >= fb_->height) return false;

    uint32_t bytes_per_scanline = fb_->pixels_per_scanline * 4;
    uint32_t scroll_bytes = bytes_per_scanline * (fb_->height - dy);

    memmove(fb_->base_address, static_cast<uint8_t*>(fb_->base_address) + bytes_per_scanline * dy, scroll_bytes);

    // neuen Bereich mit 0 füllen
    fill_rect(0, fb_->height - dy, fb_->width, dy, 0x00000000);
    return true;
}

uint32_t GopRenderDriver::screen_width_px() const {
    return fb_->width;
}
uint32_t GopRenderDriver::screen_height_px() const {
    return fb_->height;
}
uint32_t GopRenderDriver::bytes_per_scanline() const {
    return fb_->pixels_per_scanline * 4;
}
