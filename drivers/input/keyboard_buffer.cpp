#include "keyboard_buffer.h"

KEYBOARD_BUFFER keyboard_buffer = { .head = 0, .tail = 0 };

void keyboard_buffer_init() {
    keyboard_buffer.head = 0;
    keyboard_buffer.tail = 0;
}

void keyboard_buffer_write(char ch) {
    int next = (keyboard_buffer.head + 1) % BUFFER_SIZE;
    if (next != keyboard_buffer.tail) {  // Check if buffer is full
        keyboard_buffer.buffer[keyboard_buffer.head] = ch;
        keyboard_buffer.head = next;
    }
}

int keyboard_buffer_read(char *ch) {
    if (keyboard_buffer.head == keyboard_buffer.tail) {
        return 0;  // Buffer is empty
    }
    *ch = keyboard_buffer.buffer[keyboard_buffer.tail];
    keyboard_buffer.tail = (keyboard_buffer.tail + 1) % BUFFER_SIZE;
    return 1;
}