#include  <kernel/basic_renderer.h>
#include <string.h>
#include <utils.h>

#include "terminal.h"
#include "../../filesystem/devfs/devfs.h"
#include "kernel/memory.h"
#include "kernel/devices/device_manager.h"

Terminal* global_terminal;

screen_renderer::screen_renderer(Framebuffer* fb, FONT* font)
    : fb(fb), font(font)
{
    char name[5];
    DeviceManager::AllocUniqueDeviceName("fb", name, sizeof(name));
    kd = DeviceManager::RegisterGpuDevice(
        this,
        name,
        DeviceClass::Graphics,
        BusType::VIRTUAL,
        ControllerType::UefiGOP,
        nullptr
    );
    /* DevFS::register_device(fb_device);*/
}


void screen_renderer::draw_glyph_run(const GlyphRun& run)
{
    for (uint32_t i = 0; i < run.length; i++)
    {
        put_char(run.text[i], run.px + i * font->width, run.py, run.fg, run.bg);
    }
}


bool screen_renderer::fill_rect(uint32_t px, uint32_t py, uint32_t w, uint32_t h, uint32_t colour)
{
    if (px + w > fb->width || py + h > fb->height)
        return false;

    for (uint32_t y = 0; y < h; y++)
    {
        for (uint32_t x = 0; x < w; x++)
        {
            uint32_t* pix = static_cast<uint32_t*>(fb->base_address)
                + (px + x) + (py + y) * fb->pixels_per_scanline;
            *pix = colour;
        }
    }

    return true;
}

void screen_renderer::clear()
{
    fill_rect(0, 0, fb->width, fb->height, 0x00000000);
}

/*
void screen_renderer::clear_mouse_cursor(const uint8_t* mouse_cursor, const Point position) const
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


void screen_renderer::draw_overlay_mouse_cursor(const uint8_t* mouse_cursor, const Point position, const uint32_t colour)
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

void screen_renderer::put_char(char c, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color) const
{
    if (!c) return;

    uint32_t* pix_ptr = static_cast<uint32_t*>(fb->base_address);
    const char* glyph = static_cast<char*>(font->glyphBuffer) + (c * font->charsize);

    for (uint32_t row = 0; row < font->height; row++)
    {
        for (uint32_t bx = 0; bx < (font->width + 7) / 8; bx++)
        {
            uint8_t byte = glyph[row * ((font->width + 7) / 8) + bx];
            for (uint32_t bit = 0; bit < 8; bit++)
            {
                uint32_t xpix = bx * 8 + bit;
                if (xpix >= font->width) break;

                uint32_t color_to_draw = (byte & (0x80 >> bit)) ? fg_color : bg_color;
                *(pix_ptr + (x + xpix) + (y + row) * fb->pixels_per_scanline) = color_to_draw;
            }
        }
    }
}

/*
void screen_renderer::draw_cursor() const
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

void screen_renderer::clear_cursor(uint64_t x_pos, uint64_t y_pos) const
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

bool screen_renderer::scroll_pixels(int dy)
{
    if (dy <= 0 || static_cast<uint32_t>(dy) >= fb->height)
        return false;

    uint32_t bytes_per_scanline = fb->pixels_per_scanline * 4;
    uint32_t scroll_bytes = bytes_per_scanline * (fb->height - dy);

    memmove(fb->base_address,
            static_cast<uint8_t*>(fb->base_address) + bytes_per_scanline * dy,
            scroll_bytes);

    // neuen Bereich mit 0 füllen
    fill_rect(0, fb->height - dy, fb->width, dy, 0x00000000);
    return true;
}

uint32_t screen_renderer::screen_width_px() const { return fb->width; }
uint32_t screen_renderer::screen_height_px() const { return fb->height; }
