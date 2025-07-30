#include "../drivers/input/ps2/keyboard/keyboard.h"
#include "../kernel/include/basic_renderer.h"
#include "../include/string.h"
#include "shell.h"

#include "../drivers/input/ps2/keyboard/buffer.h"
#include "../drivers/input/ps2/keyboard/ps2_keyboard.h"
#include "../include/log.h"

void shell_loop(void* arg) {
    char command_buffer[input::keyboard::BUFFER_SIZE];
    int command_index = 0;
    char ch;

    Log::PrintLn("%sWelcome to VesperaOS",SHELL_PREFIX_STRING);

    while (true) {
        input::keyboard::init();
        Log::Print(SHELL_PREFIX_STRING);
        global_renderer->draw_cursor();

        while (true) {
            // Read from keyboard buffer if there is input
            if (input::keyboard::read_char(ch)) {
                Point pos = global_renderer->get_cursor_pos();
                global_renderer->clear_cursor(pos.X, pos.Y);
                if (ch == '\0') {
                    // Check if buffer is empty
                    if (command_index == 0) {
                        global_renderer->new_line();
                    } else {
                        command_buffer[command_index] = '\0';
                        global_renderer->new_line();
                        process_command(command_buffer);
                        command_index = 0;
                    }
                    break;
                } else if (ch == '\b') {
                    if (command_index > 0) {
                        command_index--;
                        global_renderer->clear_char();
                    }
                } else {
                    if (command_index < input::keyboard::BUFFER_SIZE - 1) {
                        command_buffer[command_index++] = ch;
                        global_renderer->put_char(ch);
                    }
                }
                global_renderer->draw_cursor();
            }
        }
    }
}

void process_command(const char *command) {
    if (strcmp(command, "help") == 0) {
        Log::PrintLn("Available commands:");
        Log::PrintLn(" - help: Show this help message");
        Log::PrintLn(" - clear: Clear the screen");
        Log::PrintLn("");
    } else if (strcmp(command, "clear") == 0) {
        global_renderer->clear();
    } else {
        Log::PrintLn("Unknown command: %s", command);
    }
}