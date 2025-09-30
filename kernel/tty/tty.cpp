// tty.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 09.09.25.
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

#include <basic_renderer.h>
#include "tty.h"

#include <log.h>
#include <scheduling.h>

namespace kernel::tty {
    TTY *active_tty;
    TTY tty_instances[6];
    TTYDevice *tty_devices[6];

    void tty_init(TTY *tty) {
        memset(tty->canon_buffer, 0, TTY::BUFFER_SIZE);
        tty->canonical = true;
        tty->fg = WHITE;
        tty->bg = BLACK;
    }

    void tty_handle_input(const kernel::input::InputEvent &ev) {
        if (ev.device != kernel::input::InputDeviceType::KEYBOARD) return;
        if (ev.action != kernel::input::KeyAction::PRESS) return;

        char c = ev.ascii;
        if (!c) return;

        if (c == '\b') {
            if (active_tty->canonical) {
                if (active_tty->canon_len > 0 && !active_tty->line_ready) {
                    active_tty->canon_len--;
                    global_renderer->clear_char();
                }
            } else {
                if (active_tty->raw_len > 0) {
                    active_tty->raw_len--;
                    global_renderer->clear_char();
                }
            }
            return;
        }

        if (active_tty->canonical) {
            // Canonical Mode
            if (c == '\n') {
                if (!active_tty->line_ready && active_tty->canon_len < TTY::BUFFER_SIZE - 1) {
                    active_tty->canon_buffer[active_tty->canon_len++] = '\n';
                    active_tty->canon_buffer[active_tty->canon_len] = '\0';
                    active_tty->line_ready = true;
                }
            } else {
                if (!active_tty->line_ready && active_tty->canon_len < TTY::BUFFER_SIZE - 1) {
                    active_tty->canon_buffer[active_tty->canon_len++] = c;
                    global_renderer->put_char(c);
                }
            }
        } else {
            // Non-canonical Mode
            if (active_tty->raw_len < TTY::BUFFER_SIZE - 1) {
                active_tty->raw_buffer[active_tty->raw_len++] = c;
                global_renderer->put_char(c); // Echo
            }
        }
    }

    // reference: https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797
    static Colour ansi_to_colour(int code, bool is_bg, bool bright = false) {
        switch (code) {
            case 30:
            case 40: return bright ? (Colour) 0x00808080 : BLACK; // gray for bright black
            case 31:
            case 41: return bright ? (Colour) 0x00FF6060 : RED;
            case 32:
            case 42: return bright ? (Colour) 0x0060FF60 : GREEN;
            case 33:
            case 43: return bright ? (Colour) 0x00FFFF60 : YELLOW;
            case 34:
            case 44: return bright ? (Colour) 0x0060A0FF : BLUE;
            case 35:
            case 45: return bright ? (Colour) 0x00FF60FF : MAGENTA;
            case 36:
            case 46: return bright ? (Colour) 0x0060FFFF : CYAN;
            case 37:
            case 47: return bright ? (Colour) 0x00FFFFFF : WHITE;

            case 90:
            case 100: return (Colour) 0x00808080; // Bright Black (→ Gray)
            case 91:
            case 101: return (Colour) 0x00FF6060; // Bright Red
            case 92:
            case 102: return (Colour) 0x0060FF60; // Bright Green
            case 93:
            case 103: return (Colour) 0x00FFFF60; // Bright Yellow
            case 94:
            case 104: return (Colour) 0x0060A0FF; // Bright Blue
            case 95:
            case 105: return (Colour) 0x00FF60FF; // Bright Magenta
            case 96:
            case 106: return (Colour) 0x0060FFFF; // Bright Cyan
            case 97:
            case 107: return (Colour) 0x00FFFFFF; // Bright White

            default:
                return bright ? WHITE : WHITE; // fallback
        }
    }

    static void tty_apply_sgr(TTY *tty) {
        int i = 0;
        while (i < tty->esc_param_count) {
            int code = tty->esc_params[i++];

            switch (code) {
                case 0: // Reset
                    tty->fg = WHITE;
                    tty->bg = BLACK;
                    break;

                case 30 ... 37: // Standard FG
                    tty->fg = ansi_to_colour(code, false);
                    break;
                case 40 ... 47: // Standard BG
                    tty->bg = ansi_to_colour(code, true);
                    break;

                case 90 ... 97: // Bright FG
                    tty->fg = ansi_to_colour(code, false, true);
                    break;
                case 100 ... 107: // Bright BG
                    tty->bg = ansi_to_colour(code, true, true);
                    break;

                case 38: // Extended FG
                    if (i < tty->esc_param_count && tty->esc_params[i] == 2 && i + 3 < tty->esc_param_count) {
                        int r = tty->esc_params[i + 1];
                        int g = tty->esc_params[i + 2];
                        int b = tty->esc_params[i + 3];
                        tty->fg = (Colour) ((r << 16) | (g << 8) | b);
                        i += 4;
                    }
                    break;

                case 48: // Extended BG
                    if (i < tty->esc_param_count && tty->esc_params[i] == 2 && i + 3 < tty->esc_param_count) {
                        int r = tty->esc_params[i + 1];
                        int g = tty->esc_params[i + 2];
                        int b = tty->esc_params[i + 3];
                        tty->bg = (Colour) ((r << 16) | (g << 8) | b);
                        i += 4;
                    }
                    break;

                default:
                    break;
            }
        }

        global_renderer->set_colour(tty->fg);
        global_renderer->set_bg_colour(tty->bg);
    }

    void tty_process_output(TTY *tty, char c) {
        switch (tty->esc_state) {
            case EscapeState::NONE:
                if (c == 0x1B) {
                    // ESC
                    tty->esc_state = EscapeState::ESC_RECEIVED;
                } else if (c == '\n') {
                    global_renderer->new_line();
                    tty->cursor_x = 0;
                    tty->cursor_y++;
                } else if (c == '\r') {
                    tty->cursor_x = 0;
                } else {
                    global_renderer->put_char(c);
                    tty->cursor_x++;
                }
                break;

            case EscapeState::ESC_RECEIVED:
                if (c == '[') {
                    tty->esc_state = EscapeState::CSI_RECEIVED;
                    tty->esc_param = 0;
                    tty->esc_param_count = 0;
                } else {
                    tty->esc_state = EscapeState::NONE;
                }
                break;

            case EscapeState::CSI_RECEIVED:
                if (c >= '0' && c <= '9') {
                    tty->esc_param = tty->esc_param * 10 + (c - '0');
                } else if (c == ';') {
                    if (tty->esc_param_count < TTY::MAX_PARAMS)
                        tty->esc_params[tty->esc_param_count++] = tty->esc_param;
                    tty->esc_param = 0;
                } else {
                    if (tty->esc_param_count < TTY::MAX_PARAMS)
                        tty->esc_params[tty->esc_param_count++] = tty->esc_param;

                    // CSI abschließen
                    if (c == 'm') {
                        tty_apply_sgr(tty);
                    } else if (c == 'J' && tty->esc_params[0] == 2) {
                        tty_clear(tty);
                    } else if (c == 'H') {
                        tty->cursor_x = tty->cursor_y = 0;
                        global_renderer->set_cursor({0, 0});
                    }

                    tty->esc_state = EscapeState::NONE;
                    tty->esc_param = 0;
                    tty->esc_param_count = 0;
                }
                break;
        }
    }

    void tty_clear(TTY *tty) {
        global_renderer->clear();

        tty->canon_len = 0;
        tty->line_ready = false;
        memset(tty->canon_buffer, 0, TTY::BUFFER_SIZE);

        tty->cursor_x = tty->cursor_y = 0;

        tty->esc_state = EscapeState::NONE;
        tty->esc_param = 0;
        tty->esc_param_count = 0;
    }

    size_t tty_read(char *buf, size_t count) {
        size_t read = 0;

        if (active_tty->canonical) {
            // Zeilenmodus
            while (!active_tty->line_ready) {
              //  kernel::scheduling::yield();
                asm volatile("pause");
            }

            size_t to_copy = (active_tty->canon_len < count) ? active_tty->canon_len : count;
            memcpy(buf, active_tty->canon_buffer, to_copy);
            read = to_copy;

            active_tty->canon_len = 0;
            active_tty->line_ready = false;
            memset(active_tty->canon_buffer, 0, TTY::BUFFER_SIZE);
        } else {
            // Non-Canonical (charweise)
            while (active_tty->raw_len == 0) {
            //    kernel::scheduling::yield();
                asm volatile("pause");
            }

            size_t to_copy = (active_tty->raw_len < count) ? active_tty->raw_len : count;
            memcpy(buf, active_tty->raw_buffer, to_copy);
            read = to_copy;

            memmove(active_tty->raw_buffer, active_tty->raw_buffer + to_copy,
                    active_tty->raw_len - to_copy);
            active_tty->raw_len -= to_copy;
        }

        return read;
    }
}
