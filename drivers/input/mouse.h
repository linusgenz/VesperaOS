//
// Created by linus on 05.10.24.
//

#ifndef MOUSE_H
#define MOUSE_H
#include <stdint.h>
#include "../io/io.h"
#include "../../include/graphics.h"
#include "../../kernel/include/basic_renderer.h"
#include "../../kernel/include/ScrollManager.h"

#define PS2LeftButton 0b00000001
#define PS2MiddleButton 0b00000100
#define PS2RightButton 0b00000010

#define PS2XSign 0b00010000
#define PS2YSign 0b00100000
#define PS2XOverflow 0b001000000
#define PS2YOverflow 0b100000000

extern uint8_t mouse_pointer[];

void initialize_ps2_mouse();
void handle_ps2_mouse(uint8_t data);
void process_mouse_packet();

extern Point mouse_position;

#endif //MOUSE_H