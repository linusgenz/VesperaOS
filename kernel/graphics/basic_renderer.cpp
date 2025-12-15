#include  <kernel/basic_renderer.h>
#include <string.h>
#include <utils.h>

#include "kernel/memory.h"

screen_renderer* global_renderer;

screen_renderer::screen_renderer(Framebuffer* targetFramebuffer, FONT* font)
{
    TargetFramebuffer = targetFramebuffer;
    PSF_Font = font;
    colour = WHITE;
    bg_colour = BLACK;
    cursor_position = {0, 0};
    cursor_visible = true;
}

void screen_renderer::print(const char* str)
{
    const char* chr = str;
    while (*chr != 0)
    {
        if (*chr == '\n')
        {
            new_line();
        }
        else
        {
            put_char(*chr, cursor_position.X, cursor_position.Y);
            increment_cursorX(PSF_Font->width);
            if (cursor_position.X + 8 > TargetFramebuffer->width)
            {
                new_line();
            }
        }
        chr++;
    }
}

void screen_renderer::print(const char* str, const size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        if (str[i] == '\n')
        {
            new_line();
        }
        else
        {
            put_char(str[i], cursor_position.X, cursor_position.Y);
            increment_cursorX(PSF_Font->width);
            if (cursor_position.X + PSF_Font->width > TargetFramebuffer->width)
            {
                new_line();
            }
        }
    }
}


void screen_renderer::clear()
{
    const auto fb_base = reinterpret_cast<uint64_t>(TargetFramebuffer->base_address);
    const uint64_t bytes_per_scanline = TargetFramebuffer->pixels_per_scanline * 4;
    const uint64_t fb_height = TargetFramebuffer->height;

    for (int y = 0; y < fb_height; y++)
    {
        const uint64_t pix_ptr_base = fb_base + (bytes_per_scanline * y);
        for (auto* pix_ptr = reinterpret_cast<uint32_t*>(pix_ptr_base);
             pix_ptr < reinterpret_cast<uint32_t*>(pix_ptr_base + bytes_per_scanline); pix_ptr++)
        {
            *pix_ptr = bg_colour;
        }
    }

    set_cursor({0, 0});
}

Colour screen_renderer::get_pixel(const uint32_t x, const uint32_t y) const
{
    return *reinterpret_cast<Colour*>(reinterpret_cast<uint64_t>(TargetFramebuffer->base_address) + (x * 4) + (
        y * TargetFramebuffer->pixels_per_scanline * 4));
}

void screen_renderer::clear_mouse_cursor(const uint8_t* mouse_cursor, const Point position) const
{
    /*  if (!mouse_drawn) return;

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
      }*/
}


void screen_renderer::draw_overlay_mouse_cursor(const uint8_t* mouse_cursor, const Point position, const Colour colour)
{
    /*  int32_t x_max = 16;
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
  */
}

void screen_renderer::clear_char()
{
    if (cursor_position.X == 0)
    {
        cursor_position.X = TargetFramebuffer->width;
        cursor_position.Y -= PSF_Font->height;
        if (cursor_position.Y < 0) cursor_position.Y = 0;
    }

    const uint32_t x_off = cursor_position.X;
    const uint32_t y_off = cursor_position.Y;

    auto* pixPtr = static_cast<uint32_t*>(TargetFramebuffer->base_address);
    for (unsigned long y = y_off; y < y_off + PSF_Font->height; y++)
    {
        for (unsigned long x = x_off - PSF_Font->width; x < x_off; x++)
        {
            *(pixPtr + x + y * TargetFramebuffer->pixels_per_scanline) = bg_colour;
        }
    }

    cursor_position.X -= PSF_Font->width;

    if (cursor_position.X < 0)
    {
        cursor_position.X = TargetFramebuffer->width;
        cursor_position.Y -= PSF_Font->height;
        if (cursor_position.Y < 0) cursor_position.Y = 0;
    }
}

void screen_renderer::put_char(const char chr, const uint32_t xOff, const uint32_t yOff) const
{
    if (chr == '\0') return;
    auto* pix_ptr = static_cast<uint32_t*>(TargetFramebuffer->base_address);
    const char* glyph = static_cast<char*>(PSF_Font->glyphBuffer) + (chr * PSF_Font->charsize);
    for (uint32_t y = 0; y < PSF_Font->height; y++)
    {
        for (uint32_t bx = 0; bx < (PSF_Font->width + 7) / 8; bx++)
        {
            uint8_t byte = glyph[y * ((PSF_Font->width + 7) / 8) + bx];
            for (uint32_t bit = 0; bit < 8; bit++)
            {
                uint32_t x = bx * 8 + bit;
                if (x >= PSF_Font->width) break;
                uint32_t color_to_draw = (byte & (0x80 >> bit)) ? colour : bg_colour;
                *(pix_ptr + (xOff + x) + (yOff + y) * TargetFramebuffer->pixels_per_scanline) = color_to_draw;
            }
        }
    }
}

void screen_renderer::put_char(const char chr)
{
    clear_cursor(cursor_position.X, cursor_position.Y);
    put_char(chr, cursor_position.X, cursor_position.Y);
    cursor_position.X += PSF_Font->width;
    if (cursor_position.X + PSF_Font->width > TargetFramebuffer->width)
    {
        new_line();
    }
    // draw_cursor();
}

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
}

void screen_renderer::scroll_down()
{
    const uint32_t bytes_per_scanline = TargetFramebuffer->pixels_per_scanline * 4;
    const uint32_t font_height = PSF_Font->height;

    auto* fb_base = static_cast<uint8_t*>(TargetFramebuffer->base_address);

    const size_t bytes_to_copy = bytes_per_scanline * (TargetFramebuffer->height - font_height);
    memmove(fb_base, fb_base + (bytes_per_scanline * font_height), bytes_to_copy);

    const uint32_t last_line_y = TargetFramebuffer->height - font_height;
    auto* last_line = reinterpret_cast<uint32_t*>(fb_base + (bytes_per_scanline * last_line_y));
    const size_t pixels_in_last_lines = bytes_per_scanline * font_height / 4;

    for (size_t i = 0; i < pixels_in_last_lines; i++)
    {
        last_line[i] = bg_colour;
    }
}

void screen_renderer::new_line()
{
    cursor_position.X = 0;
    cursor_position.Y += PSF_Font->height;

    if (cursor_position.Y + PSF_Font->height >= TargetFramebuffer->height)
    {
        scroll_down();
        cursor_position.Y -= PSF_Font->height;
    }
}

Point screen_renderer::get_cursor_pos() const
{
    return cursor_position;
}
