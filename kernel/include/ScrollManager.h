//
// Created by linus on 16.11.2024.
//

#ifndef SCROLLMANAGER_H
#define SCROLLMANAGER_H
#include <cstdint>
#include "../../include/graphics.h"
#include "../include/memory.h"

class BasicRenderer; // Forward declaration

struct CircularBuffer {
    uint32_t* buffer;
    uint32_t head;
    uint32_t tail;
    uint32_t lines_in_buffer;
};

struct ScrollAvailable {
    bool up = false;
    bool down = false;
};

class ScrollManager {
private:
    CircularBuffer top_buffer;
    CircularBuffer bottom_buffer;
    uint32_t max_lines_in_buffer;
    uint32_t lines_per_screen;

    Framebuffer* framebuffer;
    uint64_t bytes_per_scanline;

    BasicRenderer* renderer;
    ScrollAvailable scroll_available;

    // Optimized helper methods
    void bulk_scroll_up();
    void bulk_scroll_down();
    void bulk_memmove(void* dst, const void* src, size_t size);

    // Buffer management methods
    void save_top_lines_to_buffer();
    void save_bottom_lines_to_buffer();
    void restore_line_from_top_buffer();
    void restore_line_from_bottom_buffer();

public:
    ScrollManager(uint32_t* buffer_top, uint32_t* buffer_bottom, Framebuffer* fb, BasicRenderer* r);

    uint32_t* draw_buffer_base() const;

    void setup_new_line();
    void scroll_up();
    void scroll_down();

    void set_scroll_up(bool available);
    void set_scroll_down(bool available);

    bool can_scroll_up() const { return scroll_available.up; }
    bool can_scroll_down() const { return scroll_available.down; }
};

extern ScrollManager* scroll_manager;

#endif //SCROLLMANAGER_H
