#include "keyboard.h"
#include "keyboard_buffer.h"
#include "../../kernel/include/basic_renderer.h"

bool is_left_shift_pressed;
bool is_right_shift_pressed;

void handle_keyboard(uint8_t scancode) {
    char ascii = QWERTYKeyboard::translate(scancode, is_left_shift_pressed | is_right_shift_pressed);

    switch (scancode) {
        case left_shift:
            is_left_shift_pressed = true;
            return;
        case left_shift + 0x80:
            is_left_shift_pressed = false;
            return;
        case right_shift:
            is_right_shift_pressed = true;
            return;
        case enter:
            keyboard_buffer_write('\0');  // Null-terminate to indicate end of command
            return;
        case spacebar:
            ascii = ' ';
            break;
        case back_space:
            keyboard_buffer_write('\b');
        default:
            break;
    };

    if (ascii != 0) {
        keyboard_buffer_write(ascii);
    }
}