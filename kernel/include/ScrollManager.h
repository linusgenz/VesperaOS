//
// Created by linus on 16.11.2024.
//

#ifndef SCROLLMANAGER_H
#define SCROLLMANAGER_H
#include <cstdint>
#include "../../include/graphics.h"
#include "../include/memory.h"


class BasicRenderer;

class ScrollManager {
    public:
        ScrollManager(uint32_t* buffer_top, uint32_t* buffer_bottom, Framebuffer* fb, BasicRenderer* r, uint32_t font_height);

    private:
        struct ScrollBuffer {
            uint32_t* buffer;
            uint32_t start;
            uint32_t pos;
            uint32_t lines_in_buffer;
        };

         struct Scroll {
            bool up;
            bool down;
         };

        uint32_t f_height;

        BasicRenderer* renderer;

        ScrollBuffer top_buffer{};
        ScrollBuffer bottom_buffer{};
        Scroll scroll_available{};
        uint32_t max_lines_in_buffer;
        Framebuffer* framebuffer;
        uint32_t* framebuffer_base;
        uint32_t bytes_per_scanline;
        void save_top_lines_to_buffer();
        void save_bottom_line_to_buffer();
        void shift_lines_up() const;
        void shift_lines_down() const;
        void clear_last_line() const;
        void restore_line_from_bottom_buffer();
        void restore_line_from_top_buffer();
    public:
        void scroll_up();
        void scroll_down();
        void setup_new_line();
        bool can_scroll_up() const;
        bool can_scroll_down() const;
		void set_scroll_up(bool flag);
		void set_scroll_down(bool flag);
};

extern ScrollManager* scroll_manager;

#endif //SCROLLMANAGER_H
