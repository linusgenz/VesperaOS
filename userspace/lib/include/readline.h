// readline.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 18.03.26.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
#ifndef VESPERAOS_READLINE_H
#define VESPERAOS_READLINE_H

#include <stddef.h>

/**
 * @brief Read a line of input from the terminal with editing support.
 *
 * Displays @p prompt (if non-NULL), then reads characters from @c stdin in
 * raw, no-echo mode until the user presses Enter or Ctrl+C. The following
 * editing keys are supported during input:
 *
 * | Key                | Action                          |
 * |--------------------|---------------------------------|
 * | ← / →              | Move cursor left / right        |
 * | Home / Ctrl+A      | Move to start of line           |
 * | End  / Ctrl+E      | Move to end of line             |
 * | Backspace          | Delete character before cursor  |
 * | Delete             | Delete character under cursor   |
 * | Ctrl+K             | Kill text from cursor to EOL    |
 * | ↑ / ↓              | Walk history (32 entries, LRU)  |
 * | PageUp / PageDown  | Walk history (32 entries, LRU)  |
 * | Ctrl+C             | Cancel input, return empty line |
 *
 * The completed line (without trailing newline) is written into @p out and
 * NUL-terminated. The line is also pushed onto the internal history ring
 * buffer, deduplicating consecutive identical entries.
 *
 * On return the terminal is restored to its previous mode regardless of
 * how the function exited.
 *
 * @param prompt    Prompt string written to @c stdout before reading begins,
 *                  or @c NULL for no prompt.
 * @param out       Buffer that receives the completed line.
 * @param out_size  Size of @p out in bytes, including the NUL terminator.
 *                  Input is silently clamped to @p out_size - 1 characters.
 * @return          Number of characters written to @p out (excluding @c NUL),
 *                  or @c 0 if the input was cancelled or empty,
 *                  or @c -1 on error.
 *
 * @see tcsetattr()
 * @see tcgetattr()
 */
int readline(const char* prompt, char* out, size_t out_size);

#endif