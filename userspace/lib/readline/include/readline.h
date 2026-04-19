#ifndef VESPERAOS_READLINE_READLINE_H
#define VESPERAOS_READLINE_READLINE_H

#include <stddef.h>

/**
 * @brief Read a line of input from the terminal with editing support.
 *
 * Displays @p prompt (if non-NULL), then
 * reads characters from stdin in raw / no-echo mode until the user presses
 * Enter or Ctrl+C.
 *
 * The following editing keys are supported:
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
 * History is NOT pushed automatically.  Call add_history() after inspecting
 * the returned string if you want it recorded.
 *
 * On return the terminal is restored to its previous mode regardless of how
 * the function exited.
 *
 * @param prompt  Prompt string written to stdout before reading begins, or
 *                NULL for no prompt.
 * @return        A malloc()'d NUL-terminated string containing the line
 *                (without trailing newline).  The caller must free() it.
 *                Returns a pointer to an empty string on Ctrl+C / empty input.
 *                Returns NULL on memory allocation failure or hard read error.
 */
char* readline(const char* prompt);

/**
 * @brief Read a line into a caller-supplied buffer (VesperaOS extension).
 *
 * Identical to readline() but writes into @p out instead of allocating.
 * Also pushes the result onto the history ring buffer automatically.
 *
 * Prefer this in VesperaOS-native code where buffer lifetime is controlled
 * by the caller.
 *
 * @param prompt    Prompt string, or NULL.
 * @param out       Buffer that receives the completed line.
 * @param out_size  Size of @p out in bytes, including the NUL terminator.
 * @return          Number of characters written (excluding NUL), 0 on cancel
 *                  or empty input, -1 on error.
 */
int readline_buf(const char* prompt, char* out, size_t out_size);

/**
 * Name of the program using readline.  Set before calling readline() so that
 * any future conditional-init hooks can inspect it.
 */
extern const char* rl_readline_name;

#endif  // VESPERAOS_READLINE_READLINE_H