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
    TTYDevice* tty_devices[6];

    void tty_init(TTY* tty) {
        memset(tty, 0, sizeof(*tty));
        tty->canonical = true;
    }

    void tty_handle_input(const kernel::input::InputEvent &ev) {
        if (ev.device != kernel::input::InputDeviceType::KEYBOARD) return;
        if (ev.action != kernel::input::KeyAction::PRESS) return;

        char c = ev.ascii;
        if (!c) return;

        // Backspace
        if (c == '\b') {
            if (active_tty->canon_len > 0 && !active_tty->line_ready) {
                active_tty->canon_len--;
                global_renderer->clear_char();
            }
            return;
        }

        // Prüfe Escape-Sequenz
        if (active_tty->esc_state != EscapeState::NONE || c == 0x1B) {
            Log::debug("TTY HANDLE CHAR");
            tty_handle_char(active_tty, c);
            return;
        }

        // Normales Zeichen
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
                } else {
                    tty->esc_state = EscapeState::NONE;
                }
                break;

            case EscapeState::CSI_RECEIVED:
                if (c >= '0' && c <= '9') {
                    tty->esc_param = tty->esc_param * 10 + (c - '0');
                } else if (c == 'J') {
                    if (tty->esc_param == 2) {
                        tty_clear(tty);
                    }
                    tty->esc_state = EscapeState::NONE;
                } else if (c == 'H') {
                    tty->cursor_x = tty->cursor_y = 0;
                    global_renderer->set_cursor({0, 0});
                    tty->esc_state = EscapeState::NONE;
                } else {
                    tty->esc_state = EscapeState::NONE;
                }
                break;
        }
    }


    void tty_handle_char(TTY *tty, char c) {
        switch (tty->esc_state) {
            case EscapeState::NONE:
                if (c == 0x1B) {
                    // ESC
                    tty->esc_state = EscapeState::ESC_RECEIVED;
                } else {
                    // normales Zeichen
                    tty->canon_buffer[tty->canon_len++] = c;
                }
                break;

            case EscapeState::ESC_RECEIVED:
                if (c == '[') {
                    tty->esc_state = EscapeState::CSI_RECEIVED;
                } else {
                    tty->esc_state = EscapeState::NONE;
                }
                break;

            case EscapeState::CSI_RECEIVED:
                if (c >= '0' && c <= '9') {
                    tty->esc_param = c - '0';
                } else if (c == 'J') {
                    if (tty->esc_param == 2) {
                        tty_clear(tty);
                    }
                    tty->esc_state = EscapeState::NONE;
                    tty->esc_param = 0;
                } else {
                    tty->esc_state = EscapeState::NONE;
                    tty->esc_param = 0;
                }
                break;
        }
    }

    void tty_clear(TTY *tty) {
        global_renderer->clear(); // Renderer leeren

        tty->canon_len = 0;
        tty->line_ready = false;
        memset(tty->canon_buffer, 0, TTY::BUFFER_SIZE);

        tty->cursor_x = tty->cursor_y = 0;

        tty->esc_state = EscapeState::NONE;
        tty->esc_param = 0;
    }

    size_t tty_read(char *buf, size_t count) {
        size_t read = 0;

        while (true) {
            if (!active_tty->line_ready) {
                //    kernel::scheduling::yield();
                continue;
            }

            size_t to_copy = (active_tty->canon_len < count) ? active_tty->canon_len : count;
            for (size_t i = 0; i < to_copy; i++) {
                buf[i] = active_tty->canon_buffer[i];
            }
            read = to_copy;

            active_tty->canon_len = 0;
            active_tty->line_ready = false;
            memset(active_tty->canon_buffer, 0, TTY::BUFFER_SIZE);
            break;
        }

        return read;
    }
}
