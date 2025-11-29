//
// Created by linus on 16.11.2024.
//

#include <kernel/ScrollManager.h>

#include "../../drivers/ps2/mouse/mouse.h"
#include <kernel/basic_renderer.h>
#include <kernel/memory.h>

ScrollManager* scroll_manager;

ScrollManager::ScrollManager(uint64_t* buffer_top, uint64_t* buffer_bottom, Framebuffer* fb, BasicRenderer* r,
                             uint32_t font_height)
{
    top_buffer = {buffer_top, 0, 0, 0};
    bottom_buffer = {buffer_bottom, 0, 0, 0};
    max_lines_in_buffer = fb->height * 5;
    framebuffer = fb;
    framebuffer_base = static_cast<uint32_t*>(fb->base_address);
    bytes_per_scanline = fb->pixels_per_scanline * sizeof(uint32_t);
    renderer = r;
    f_height = font_height;
}

void ScrollManager::setup_new_line()
{
    global_renderer->clear_mouse_cursor(input::mouse::pointer, input::mouse::get_position());

    // scroll to the actual bottom, of the actual window
    while (bottom_buffer.lines_in_buffer > 0)
    {
        scroll_down();
    }

    //  save_top_lines_to_buffer();

    shift_lines_up();

    clear_last_line();

    // Update scroll state
    scroll_available.down = false;
    scroll_available.up = true;

    renderer->draw_overlay_mouse_cursor(input::mouse::pointer, input::mouse::get_position(), Colour::WHITE);
}

// TODO make this better
void ScrollManager::scroll_down()
{
    if (bottom_buffer.lines_in_buffer <= 0)
    {
        return;
    }
    scroll_available.up = true;

    renderer->clear_mouse_cursor(input::mouse::pointer, input::mouse::get_position());

    save_top_lines_to_buffer();

    shift_lines_up();

    restore_line_from_bottom_buffer();

    renderer->draw_overlay_mouse_cursor(input::mouse::pointer, input::mouse::get_position(), Colour::WHITE);
    if (bottom_buffer.lines_in_buffer <= 0)
    {
        scroll_available.down = false;
    }
}

void ScrollManager::scroll_up()
{
    Point pos = renderer->get_cursor_pos();
    if (bottom_buffer.lines_in_buffer == 0)
    {
        renderer->clear_cursor(pos.X, pos.Y);
    }
    if (top_buffer.lines_in_buffer <= 0)
    {
        scroll_available.up = false;
        return;
    }
    scroll_available.down = true;

    renderer->clear_mouse_cursor(input::mouse::pointer, input::mouse::get_position());

    save_bottom_line_to_buffer();

    shift_lines_down();

    restore_line_from_top_buffer();

    renderer->draw_overlay_mouse_cursor(input::mouse::pointer, input::mouse::get_position(), Colour::WHITE);
}

void ScrollManager::restore_line_from_bottom_buffer()
{
    uint32_t screen_width = framebuffer->width;
    uint32_t screen_height = framebuffer->height;

    bottom_buffer.pos =
        (bottom_buffer.start - f_height + max_lines_in_buffer) % max_lines_in_buffer;

    uint32_t* bottom_line_addr = framebuffer_base + screen_width * (screen_height - f_height);
    memcpy(bottom_line_addr,
           &bottom_buffer.buffer[bottom_buffer.pos * screen_width],
           bytes_per_scanline * f_height);

    bottom_buffer.start = bottom_buffer.pos;
    bottom_buffer.lines_in_buffer--;
}

void ScrollManager::restore_line_from_top_buffer()
{
    uint32_t screen_width = framebuffer->width;

    top_buffer.pos =
        (top_buffer.start - f_height + max_lines_in_buffer) % max_lines_in_buffer;

    memcpy(framebuffer_base,
           &top_buffer.buffer[top_buffer.pos * screen_width],
           bytes_per_scanline * f_height);

    top_buffer.start = top_buffer.pos;
    top_buffer.lines_in_buffer--;
}

void ScrollManager::save_bottom_line_to_buffer()
{
    uint32_t screen_width = framebuffer->width;
    uint32_t screen_height = framebuffer->height;

    for (uint32_t i = 0; i < f_height; i++)
    {
        uint32_t bottom_line_index = screen_height - f_height + i;
        bottom_buffer.pos =
            (bottom_buffer.start + i) % max_lines_in_buffer;

        memcpy(&bottom_buffer.buffer[bottom_buffer.pos * screen_width],
               framebuffer_base + bottom_line_index * screen_width,
               bytes_per_scanline);
    }

    bottom_buffer.start = (bottom_buffer.start + f_height) % max_lines_in_buffer;
    bottom_buffer.lines_in_buffer++;
}

void ScrollManager::save_top_lines_to_buffer()
{
    uint32_t screen_width = framebuffer->width;

    for (uint32_t i = 0; i < f_height; i++)
    {
        uint32_t top_buffer_index =
            (top_buffer.start + i) % max_lines_in_buffer;

        memcpy(&top_buffer.buffer[top_buffer_index * screen_width],
               framebuffer_base + i * screen_width,
               bytes_per_scanline);
    }

    top_buffer.lines_in_buffer++;
    top_buffer.start = (top_buffer.start + f_height) % max_lines_in_buffer;
}

void ScrollManager::shift_lines_up() const
{
    uint32_t screen_width = framebuffer->width;
    uint32_t screen_height = framebuffer->height;

    for (uint32_t y = 0; y < screen_height - f_height; y++)
    {
        uint32_t* src_offset = framebuffer_base + (y + f_height) * screen_width;
        uint32_t* dest_offset = framebuffer_base + y * screen_width;

        memcpy(dest_offset, src_offset, bytes_per_scanline);
    }
}

void ScrollManager::shift_lines_down() const
{
    uint32_t screen_width = framebuffer->width;
    uint32_t screen_height = framebuffer->height;

    for (int64_t y = screen_height - 1; y >= f_height; y--)
    {
        uint32_t* src_offset = framebuffer_base + (y - f_height) * screen_width;
        uint32_t* dest_offset = framebuffer_base + y * screen_width;

        memcpy(dest_offset, src_offset, bytes_per_scanline);
    }
}

void ScrollManager::clear_last_line() const
{
    uint32_t screen_width = framebuffer->width;
    uint32_t screen_height = framebuffer->height;

    uint32_t* clear_line_addr = framebuffer_base + screen_width * (screen_height - f_height);
    memset(clear_line_addr, 0, bytes_per_scanline * f_height);
}

bool ScrollManager::can_scroll_up() const
{
    return scroll_available.up;
}

bool ScrollManager::can_scroll_down() const
{
    return scroll_available.down;
}

void ScrollManager::set_scroll_up(bool flag)
{
    scroll_available.up = flag;
}

void ScrollManager::set_scroll_down(bool flag)
{
    scroll_available.down = flag;
}

// TODO fix bug where when bottom buffer restored remainder blacks half of the line
