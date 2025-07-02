#include "mouse.h"

uint8_t mouse_pointer[] = {
    0b10000000, 0b00000000,
    0b11000000, 0b00000000,
    0b11100000, 0b00000000,
    0b11110000, 0b00000000,
    0b11111000, 0b00000000,
    0b11111100, 0b00000000,
    0b11111110, 0b00000000,
    0b11111111, 0b00000000,
    0b11111111, 0b10000000,
    0b11111111, 0b11000000,
    0b11111111, 0b11100000,
    0b11111111, 0b10000000,
    0b11111111, 0b00000000,
    0b11000111, 0b00000000,
    0b00000011, 0b00000000,
    0b00000001, 0b00000000,
};
Point mouse_position;
/*
void mouse_wait() {
    uint64_t timeout = 100000;
    while (timeout--) {    
        if ((inb(0x64) & 0b10) == 0) {
            return;
        }
    }
}

void mouse_wait_input() {
    uint64_t timeout = 100000;
    while (timeout--) {    
        if (inb(0x64) & 0b1) {
            return;
        }
    }
}

void mouse_write(uint8_t value) {
    mouse_wait();
    outb(0x64, 0xD4);
    mouse_wait();
    outb(0x60, value);
}

uint8_t mouse_read() {
    mouse_wait_input();
    return inb(0x60);
}

uint8_t mouse_cycle = 0;
uint8_t mouse_packet[4];
bool mouse_packet_ready = false;
Point mouse_position;
Point mouse_position_old;
void handle_ps2_mouse(uint8_t data) {

    static bool skip = true;
    if (skip) {skip = false; return;}

    switch (mouse_cycle) {
        case 0:
            if ((data & 0b00001000) == 0) break;
            mouse_packet[0] = data;
            mouse_cycle++;
            break;
        case 1:
            mouse_packet[1] = data;
            mouse_cycle++;
            break;
        case 2:
            mouse_packet[2] = data;
            mouse_cycle++;
            break;
        case 3:
            mouse_packet[3] = data;
            mouse_packet_ready = true;
            mouse_cycle = 0;
            break;
    }

    if (mouse_packet_ready) {
        process_mouse_packet();
        mouse_packet_ready = false;
    }
}

void process_mouse_packet() {
    if (!mouse_packet_ready) return;
        mouse_packet_ready = false;


    bool x_negative = mouse_packet[0] & PS2XSign;
    bool y_negative = mouse_packet[0] & PS2YSign;
    bool x_overflow = mouse_packet[0] & PS2XOverflow;
    bool y_overflow = mouse_packet[0] & PS2YOverflow;

    // Calculate X movement
    if (!x_negative) {
        mouse_position.X += mouse_packet[1];
        if (x_overflow) mouse_position.X += 255;
    } else {
        mouse_packet[1] = 256 - mouse_packet[1];
        mouse_position.X -= mouse_packet[1];
        if (x_overflow) mouse_position.X -= 255;
    }

    // Calculate Y movement
    if (!y_negative) {
        mouse_position.Y -= mouse_packet[2];
        if (y_overflow) mouse_position.Y -= 255;
    } else {
        mouse_packet[2] = 256 - mouse_packet[2];
        mouse_position.Y += mouse_packet[2];
        if (y_overflow) mouse_position.Y += 255;
    }

        if (mouse_position.X < 0) mouse_position.X = 0;
        if (mouse_position.X > global_renderer->TargetFramebuffer->width-1) mouse_position.X = global_renderer->TargetFramebuffer->width-1;

        if (mouse_position.Y < 0) mouse_position.Y = 0;
        if (mouse_position.Y > global_renderer->TargetFramebuffer->height-1) mouse_position.Y = global_renderer->TargetFramebuffer->height-1;

        int8_t wheel_movement = (int8_t)mouse_packet[3];
        if (wheel_movement > 0) {
            if (scroll_manager->can_scroll_down()) {
                scroll_manager->scroll_down();
                if (!scroll_manager->can_scroll_down())
                {
                    global_renderer->draw_cursor();
                }
            }
        } else if (wheel_movement < 0) {
            if (scroll_manager->can_scroll_up()) {
                scroll_manager->scroll_up();
            }
        }

        global_renderer->clear_mouse_cursor(mouse_pointer, mouse_position_old);
        global_renderer->draw_overlay_mouse_cursor(mouse_pointer, mouse_position, Colour::WHITE);

        if (mouse_packet[0] & PS2LeftButton) {

        }
        if (mouse_packet[0] & PS2MiddleButton) {

        }
        if (mouse_packet[0] & PS2RightButton) {

        }
        
        mouse_packet_ready = false;
        mouse_position_old = mouse_position;
}

void initialize_ps2_mouse() {
    outb(0x64, 0xA8); // enabling he auxiliary device - mouse

    mouse_wait();
    outb(0x64, 0x20);
    mouse_wait_input();
    uint8_t status = inb(0x60);
    status |= 0b10;
    mouse_wait();
    outb(0x64, 0x60);
    mouse_wait();
    outb(0x60, status); // setting correct bit is the "compaq" status byte https://wiki.osdev.org/PS/2_Mouse

    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF4);
    mouse_read();

    // Enable IntelliMouse mode for four-byte packet support (for scroll wheel)
    mouse_write(0xF3); // Set sample rate
    mouse_write(200);  // First part of IntelliMouse activation sequence
    mouse_write(0xF3);
    mouse_write(100);
    mouse_write(0xF3);
    mouse_write(80);
}
*/