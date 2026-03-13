#include "gop_render_driver.h"

#include <vespera/cpu/simd.h>
#include <vespera/cpu/simd_mem.h>
#include <vespera/mm/memory.h>
#include <vespera/terminal.h>

#include "../../filesystem/devfs/devfs.h"
#include "vespera/devices/device_manager.h"
#include "vespera/log.h"

static void* scalar_memcpy(void* dst, const void* src, usize len) {
    auto* d = static_cast<u8*>(dst);
    const auto* s = static_cast<const u8*>(src);
    while (len--) *d++ = *s++;
    return dst;
}

static void scalar_memset_u32(void* dst, u32 value, usize len_bytes) {
    auto* d = static_cast<u32*>(dst);
    usize count = len_bytes / 4;
    while (count--) *d++ = value;
}

static void* scalar_memmove(void* dst, const void* src, usize len) {
    auto* d = static_cast<u8*>(dst);
    const auto* s = static_cast<const u8*>(src);
    if (d < s) {
        while (len--) *d++ = *s++;
    } else {
        d += len;
        s += len;
        while (len--) *--d = *--s;
    }
    return dst;
}

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
    init_simd();
}

void GopRenderDriver::init_simd() noexcept {
    const auto& f = simd_features();
    if (f.avx2) {
        using_avx = true;
        fn_memcpy_ = fb_memcpy_avx2;
        fn_memset_ = fb_memset_avx2;
        fn_memmove_ = fb_memmove_avx2;
    } else if (f.sse2) {
        using_sse = true;
        fn_memcpy_ = fb_memcpy_sse2;
        fn_memset_ = fb_memset_sse2;
        fn_memmove_ = scalar_memmove;
    } else {
        fn_memcpy_ = scalar_memcpy;
        fn_memset_ = scalar_memset_u32;
        fn_memmove_ = scalar_memmove;
    }
}

void GopRenderDriver::draw_glyph_run(const GlyphRun& run) {
    for (u32 i = 0; i < run.length; i++) {
        put_char(run.text[i], run.px + i * font_->width, run.py, run.fg, run.bg);
    }
}

bool GopRenderDriver::fill_rect(u32 px, u32 py, u32 w, u32 h, u32 colour) {
    if (px + w > fb_->width || py + h > fb_->height) return false;

    for (u32 y = 0; y < h; y++) {
        u32* row = static_cast<u32*>(fb_->base_address) + (py + y) * fb_->pixels_per_scanline + px;
        fn_memset_(row, colour, w * sizeof(u32));
    }

    return true;
}

void GopRenderDriver::clear() {
    fill_rect(0, 0, fb_->width, fb_->height, 0x00000000);
}

void GopRenderDriver::flush() {
    asm volatile("sfence" ::: "memory");
    if (simd_features().avx) {
        asm volatile("vzeroupper");
    }
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

bool GopRenderDriver::blit_buffer(const void* pixels, u32 buffer_width, u32 buffer_height, u32 dst_x, u32 dst_y) {
    if (!pixels) return false;
    if (dst_x >= fb_->width || dst_y >= fb_->height) return false;

    u32 max_w = (dst_x + buffer_width > fb_->width) ? fb_->width - dst_x : buffer_width;
    u32 max_h = (dst_y + buffer_height > fb_->height) ? fb_->height - dst_y : buffer_height;

    const auto* src = static_cast<const u32*>(pixels);
    auto* dst = static_cast<u32*>(fb_->base_address);

    for (u32 y = 0; y < max_h; y++) {
        fn_memcpy_(dst + (dst_y + y) * fb_->pixels_per_scanline + dst_x, src + y * buffer_width, max_w * sizeof(u32));
    }
    return true;
}

bool GopRenderDriver::scroll_pixels(int dy) {
    if (dy <= 0 || static_cast<u32>(dy) >= fb_->height) return false;

    u32 bps = fb_->pixels_per_scanline * 4;
    fn_memmove_(fb_->base_address, static_cast<u8*>(fb_->base_address) + bps * dy, bps * (fb_->height - dy));
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
