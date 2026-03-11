#include "gop_render_driver.h"

#include <vespera/mm/memory.h>
#include <vespera/terminal.h>

#include "../../filesystem/devfs/devfs.h"
#include "vespera/devices/device_manager.h"

GopRenderDriver::GopRenderDriver(framebuffer_t* fb, font_t* font)
    : fb_(fb)
    , font_(font) {
    char name[10];
    DeviceManager::alloc_unique_device_name("uefi_gop", name, sizeof(name));
    kd_ = DeviceManager::register_device(
        DeviceDescriptor{}
            .set_name(name)
            .set_type(DeviceType::Gpu)
            .set_class(DeviceClass::Graphics)
            .set_bus(BusType::VIRTUAL)
            .set_controller(ControllerType::UefiGop)
            .with_gpu(this)
    );
    DevFs::register_device(kd_);
}

void GopRenderDriver::draw_glyph_run(const GlyphRun& run) {
    for (u32 i = 0; i < run.length; i++) {
        put_char(run.text[i], run.px + i * font_->width, run.py, run.fg, run.bg);
    }
}

bool GopRenderDriver::fill_rect(u32 px, u32 py, u32 w, u32 h, u32 colour) {
    if (px + w > fb_->width || py + h > fb_->height) return false;

    for (u32 y = 0; y < h; y++) {
        for (u32 x = 0; x < w; x++) {
            u32* pix = static_cast<u32*>(fb_->base_address) + (px + x) + (py + y) * fb_->pixels_per_scanline;
            *pix = colour;
        }
    }

    return true;
}

void GopRenderDriver::clear() {
    fill_rect(0, 0, fb_->width, fb_->height, 0x00000000);
}

/*
void gop_render_driver::clear_mouse_cursor(const u8* mouse_cursor, const Point position) const
{
      if (!mouse_drawn) return;

      i32 x_max = 16;
      i32 y_max = 16;
      i32 diffrence_x = TargetFramebuffer->width - position.X;
      i32 diffrence_y = TargetFramebuffer->height - position.Y;

      if (diffrence_x < 16) x_max = diffrence_x;
      if (diffrence_y < 16) y_max = diffrence_y;

      for (i32 y = 0; y < y_max; y++)
      {
          for (i32 x = 0; x < x_max; x++)
          {
              i32 bit = y * 16 + x;
              i32 byte = bit / 8;
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


void gop_render_driver::draw_overlay_mouse_cursor(const u8* mouse_cursor, const Point position, const u32
colour)
{
      i32 x_max = 16;
      i32 y_max = 16;
      i32 diffrence_x = TargetFramebuffer->width - position.X;
      i32 diffrence_y = TargetFramebuffer->height - position.Y;

      if (diffrence_x < 16) x_max = diffrence_x;
      if (diffrence_y < 16) y_max = diffrence_y;

      for (i32 y = 0; y < y_max; y++)
      {
          for (i32 x = 0; x < x_max; x++)
          {
              i32 bit = y * 16 + x;
              i32 byte = bit / 8;
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

void GopRenderDriver::put_char(char c, u32 x, u32 y, u32 fg_color, u32 bg_color) const {
    if (c >= ((psf2_header_t*)font_->header)->length) c = '?';
    if (!c) return;

    auto pix_ptr = static_cast<u32*>(fb_->base_address);
    const char* glyph = static_cast<char*>(font_->glyph_buffer) + (c * font_->charsize);

    for (u32 row = 0; row < font_->height; row++) {
        for (u32 bx = 0; bx < (font_->width + 7) / 8; bx++) {
            u8 byte = glyph[row * ((font_->width + 7) / 8) + bx];
            for (u32 bit = 0; bit < 8; bit++) {
                u32 xpix = bx * 8 + bit;
                if (xpix >= font_->width) break;

                u32 color_to_draw = (byte & (0x80 >> bit)) ? fg_color : bg_color;
                *(pix_ptr + (x + xpix) + (y + row) * fb_->pixels_per_scanline) = color_to_draw;
            }
        }
    }
}

/*
void gop_render_driver::draw_cursor() const
{
    auto* pix_ptr = static_cast<u32*>(TargetFramebuffer->base_address);

    u64 max_y = min(cursor_position.Y + PSF_Font->height, TargetFramebuffer->height);
    u64 max_x = min(cursor_position.X + PSF_Font->width, TargetFramebuffer->width);

    for (u64 y = cursor_position.Y; y < max_y; y++)
    {
        for (u64 x = cursor_position.X; x < max_x; x++)
        {
            *(pix_ptr + x + y * TargetFramebuffer->pixels_per_scanline) = WHITE;
        }
    }
}

void gop_render_driver::clear_cursor(u64 x_pos, u64 y_pos) const
{
    auto* pix_ptr = static_cast<u32*>(TargetFramebuffer->base_address);

    u64 max_y = min(y_pos + PSF_Font->height, TargetFramebuffer->height);
    u64 max_x = min(x_pos + PSF_Font->width, TargetFramebuffer->width);

    for (u64 y = y_pos; y < max_y; y++)
    {
        for (u64 x = x_pos; x < max_x; x++)
        {
            *(pix_ptr + x + y * TargetFramebuffer->pixels_per_scanline) = BLACK;
        }
    }
}*/

bool GopRenderDriver::blit_buffer(const void* pixels, u32 buffer_width, u32 buffer_height, u32 dst_x, u32 dst_y) {
    if (!pixels) return false;

    u32 max_w = buffer_width;
    u32 max_h = buffer_height;

    if (dst_x >= fb_->width || dst_y >= fb_->height) return false;

    if (dst_x + buffer_width > fb_->width) max_w = fb_->width - dst_x;
    if (dst_y + buffer_height > fb_->height) max_h = fb_->height - dst_y;

    const auto* src = static_cast<const u32*>(pixels);
    auto* dst = static_cast<u32*>(fb_->base_address);

    for (u32 y = 0; y < max_h; y++) {
        u32* dst_row = dst + (dst_y + y) * fb_->pixels_per_scanline + dst_x;
        const u32* src_row = src + y * buffer_width;

        memcpy(dst_row, src_row, max_w * sizeof(u32));
    }

    return true;
}

bool GopRenderDriver::scroll_pixels(int dy) {
    if (dy <= 0 || static_cast<u32>(dy) >= fb_->height) return false;

    u32 bytes_per_scanline = fb_->pixels_per_scanline * 4;
    u32 scroll_bytes = bytes_per_scanline * (fb_->height - dy);

    memmove(fb_->base_address, static_cast<u8*>(fb_->base_address) + bytes_per_scanline * dy, scroll_bytes);

    // neuen Bereich mit 0 füllen
    fill_rect(0, fb_->height - dy, fb_->width, dy, 0x00000000);
    return true;
}

u32 GopRenderDriver::screen_width_px() const {
    return fb_->width;
}
u32 GopRenderDriver::screen_height_px() const {
    return fb_->height;
}
u32 GopRenderDriver::bytes_per_scanline() const {
    return fb_->pixels_per_scanline * 4;
}
