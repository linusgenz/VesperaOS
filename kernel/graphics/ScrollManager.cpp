//
// Created by linus on 16.11.2024.
//

#include "../include/ScrollManager.h"
#include "../../drivers/input/ps2/mouse/mouse.h"
#include "../include/basic_renderer.h"

ScrollManager *scroll_manager = nullptr;

ScrollManager::ScrollManager(uint32_t *buffer_top, uint32_t *buffer_bottom, Framebuffer *fb, BasicRenderer *r) {
    // Keep the per-line circular buffers as before (used for history)
    top_buffer = {buffer_top, 0, 0, 0};
    bottom_buffer = {buffer_bottom, 0, 0, 0};
    max_lines_in_buffer = fb->height * 5; // same heuristic

    bytes_per_scanline = fb->pixels_per_scanline * sizeof(uint32_t);
    framebuffer = fb;
    renderer = r;

    // Calculate lines per screen (assuming 16 pixel high characters)
    lines_per_screen = fb->height / 16;
}

uint32_t *ScrollManager::draw_buffer_base() const {
    return renderer->get_draw_buffer();
}

inline void fast_memmove(uint32_t* dest, const uint32_t* src, size_t bytes) {
    if (dest == src || bytes == 0) return;

    size_t pixels = bytes / sizeof(uint32_t);

    if (dest < src) {
        size_t chunks64 = pixels / 2;
        uint64_t* d64 = reinterpret_cast<uint64_t*>(dest);
        const uint64_t* s64 = reinterpret_cast<const uint64_t*>(src);
        for (size_t i = 0; i < chunks64; i++) {
            d64[i] = s64[i];
        }
        if (pixels % 2) {
            dest[pixels - 1] = src[pixels - 1];
        }
    } else {
        size_t chunks64 = pixels / 2;
        uint64_t* d64 = reinterpret_cast<uint64_t*>(dest + (pixels - pixels % 2 - 2));
        const uint64_t* s64 = reinterpret_cast<const uint64_t*>(src + (pixels - pixels % 2 - 2));
        for (size_t i = 0; i < chunks64; i++) {
            d64[-(int)i] = s64[-(int)i];
        }
        if (pixels % 2) {
            dest[0] = src[0];
        }
    }
}


void ScrollManager::setup_new_line() {
    global_renderer->clear_mouse_cursor(input::mouse::pointer, input::mouse::get_position());

    // Scroll to the actual bottom of the actual window
    while (bottom_buffer.lines_in_buffer > 0) {
        scroll_down();
    }

    save_top_lines_to_buffer();

    // Use optimized bulk scroll instead of line-by-line
    bulk_scroll_up();

    // Clear the last line using optimized method
    renderer->clear_line(lines_per_screen - 1);

    // Update scroll state
    scroll_available.down = false;
    scroll_available.up = true;

    // Present the updated frame
    renderer->present();

    renderer->draw_overlay_mouse_cursor(input::mouse::pointer, input::mouse::get_position(), Colour::WHITE);
}

void ScrollManager::bulk_scroll_up() {
    uint32_t* buffer = renderer->get_draw_buffer();
    size_t pixels_per_line = 16 * framebuffer->width;
    size_t bytes_to_move = (lines_per_screen - 1) * pixels_per_line * sizeof(uint32_t);

    // Ganzes Block nach oben verschieben
    fast_memmove(buffer, buffer + pixels_per_line, bytes_to_move);

    // Letzte Zeile löschen
    renderer->clear_line(lines_per_screen - 1);
}


void ScrollManager::scroll_down() {
    if (bottom_buffer.lines_in_buffer <= 0) {
        return;
    }
    scroll_available.up = true;

    renderer->clear_mouse_cursor(input::mouse::pointer, input::mouse::get_position());

    save_top_lines_to_buffer();

    bulk_scroll_up();

    restore_line_from_bottom_buffer();

    renderer->present();

    renderer->draw_overlay_mouse_cursor(input::mouse::pointer, input::mouse::get_position(), Colour::WHITE);

    if (bottom_buffer.lines_in_buffer <= 0) {
        scroll_available.down = false;
    }
}

void ScrollManager::scroll_up() {
    Point pos = renderer->get_cursor_pos();
    if (bottom_buffer.lines_in_buffer == 0) {
        renderer->clear_cursor(pos.X, pos.Y);
    }
    if (top_buffer.lines_in_buffer <= 0) {
        scroll_available.up = false;
        return;
    }
    scroll_available.down = true;

    renderer->clear_mouse_cursor(input::mouse::pointer, input::mouse::get_position());

    // Save bottom line to buffer before scrolling
    save_bottom_lines_to_buffer();

    // Use optimized bulk scroll down
    bulk_scroll_down();

    // Restore top line from buffer
    restore_line_from_top_buffer();

    // Present the updated frame
    renderer->present();

    renderer->draw_overlay_mouse_cursor(input::mouse::pointer, input::mouse::get_position(), Colour::WHITE);
}

void ScrollManager::bulk_scroll_down() {
    uint32_t* buffer = renderer->get_draw_buffer();
    size_t pixels_per_line = 16 * framebuffer->width;
    size_t bytes_to_move = (lines_per_screen - 1) * pixels_per_line * sizeof(uint32_t);

    fast_memmove(buffer + pixels_per_line, buffer, bytes_to_move);

    renderer->clear_line(0);
}


void ScrollManager::save_top_lines_to_buffer() {
    // Save the top line before it gets scrolled away
    if (top_buffer.lines_in_buffer >= max_lines_in_buffer) {
        // Buffer is full, overwrite oldest line
        top_buffer.head = (top_buffer.head + 1) % max_lines_in_buffer;
    } else {
        top_buffer.lines_in_buffer++;
    }

    // Copy the top line to buffer
    uint32_t* buffer = renderer->get_draw_buffer();
    uint32_t pixels_per_line = 16 * framebuffer->width;
    size_t line_bytes = pixels_per_line * sizeof(uint32_t);

    uint32_t* buffer_pos = top_buffer.buffer + (top_buffer.tail * pixels_per_line);
    renderer->fast_memcpy(buffer_pos, buffer, line_bytes);

    top_buffer.tail = (top_buffer.tail + 1) % max_lines_in_buffer;
}

void ScrollManager::save_bottom_lines_to_buffer() {
    // Save the bottom line before scrolling up
    if (bottom_buffer.lines_in_buffer >= max_lines_in_buffer) {
        bottom_buffer.head = (bottom_buffer.head + 1) % max_lines_in_buffer;
    } else {
        bottom_buffer.lines_in_buffer++;
    }

    uint32_t* buffer = renderer->get_draw_buffer();
    uint32_t pixels_per_line = 16 * framebuffer->width;
    uint32_t* last_line = buffer + ((lines_per_screen - 1) * pixels_per_line);
    size_t line_bytes = pixels_per_line * sizeof(uint32_t);

    uint32_t* buffer_pos = bottom_buffer.buffer + (bottom_buffer.tail * pixels_per_line);
    renderer->fast_memcpy(buffer_pos, last_line, line_bytes);

    bottom_buffer.tail = (bottom_buffer.tail + 1) % max_lines_in_buffer;
}

void ScrollManager::restore_line_from_top_buffer() {
    if (top_buffer.lines_in_buffer <= 0) return;

    // Restore top line from buffer
    uint32_t* buffer = renderer->get_draw_buffer();
    uint32_t pixels_per_line = 16 * framebuffer->width;
    size_t line_bytes = pixels_per_line * sizeof(uint32_t);

    top_buffer.head = (top_buffer.head - 1 + max_lines_in_buffer) % max_lines_in_buffer;
    uint32_t* buffer_pos = top_buffer.buffer + (top_buffer.head * pixels_per_line);

    renderer->fast_memcpy(buffer, buffer_pos, line_bytes);

    top_buffer.lines_in_buffer--;
}

void ScrollManager::restore_line_from_bottom_buffer() {
    if (bottom_buffer.lines_in_buffer <= 0) return;

    // Restore bottom line from buffer
    uint32_t* buffer = renderer->get_draw_buffer();
    uint32_t pixels_per_line = 16 * framebuffer->width;
    uint32_t* last_line = buffer + ((lines_per_screen - 1) * pixels_per_line);
    size_t line_bytes = pixels_per_line * sizeof(uint32_t);

    bottom_buffer.tail = (bottom_buffer.tail - 1 + max_lines_in_buffer) % max_lines_in_buffer;
    uint32_t* buffer_pos = bottom_buffer.buffer + (bottom_buffer.tail * pixels_per_line);

    renderer->fast_memcpy(last_line, buffer_pos, line_bytes);

    bottom_buffer.lines_in_buffer--;
}

void ScrollManager::set_scroll_up(bool available) {
    scroll_available.up = available;
}

void ScrollManager::set_scroll_down(bool available) {
    scroll_available.down = available;
}