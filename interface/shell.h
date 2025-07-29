//
// Created by linus on 08.11.2024.
//

#ifndef SHELL_H
#define SHELL_H

#define SHELL_PREFIX_STRING " > "
void process_command(const char *command);
void shell_loop(void* arg);

#endif //SHELL_H
