//
// Created by linus on 08.11.24.
//

#ifndef KEYBOARD_BUFFER_H
#define KEYBOARD_BUFFER_H
#include <stdint.h>

#define BUFFER_SIZE 128

typedef struct {
    char buffer[BUFFER_SIZE];
    int head;  // Write position
    int tail;  // Read position
} KEYBOARD_BUFFER;

extern KEYBOARD_BUFFER keyboard_buffer;
void keyboard_buffer_init();
void keyboard_buffer_write(char ch);
int keyboard_buffer_read(char *ch);

#endif //KEYBOARD_BUFFER_H