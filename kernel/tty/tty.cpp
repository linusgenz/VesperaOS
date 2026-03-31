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

#include <vespera/graphics/colors.h>
#include <vespera/input/keycode.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera/signals.h>
#include <vespera/tty/tty.h>

namespace kernel::tty {
    /**
     * @brief Points to the TTY instance that currently holds keyboard focus.
     *
     * This pointer determines which TTY receives characters from keyboard
     * interrupt events. It is set during TTY initialization and updated
     * when the user switches the tty.
     *
     * @note This is strictly an input-routing concept and has no relation
     *       to which realm or unit is currently scheduled. The keyboard
     *       interrupt fires asynchronously — the unit running at interrupt
     *       time is arbitrary and must never be used to infer focus.
     *
     * @note This is intentionally separate from the per-realm TTY handle
     *       stored in each realm's handle table. Read access control
     *       (i.e. which realm may consume TTY input) is governed by
     *       TTY::fg_realm_id, not by this pointer.
     *
     * @see TTY::fg_realm_id
     * @see tty_handle_input()
     */
    TTY* keyboard_focus_tty;
    TTY tty_instances[6];
    TtyDevice* tty_devices[6];

    void tty_init(TTY* tty, Terminal* term) {
        memset(tty->canon_buffer, 0, TTY::BUFFER_SIZE);

        tty->canonical = true;
        tty->fg = WHITE;
        tty->bg = BLACK;
        tty->term = term;
        tty->utf8 = {};

        term->set_colour(tty->fg, tty->bg);
    }

    void tty_handle_input(const input::InputEvent& ev) {
        if (ev.device != input::InputDeviceType::KEYBOARD) return;
        if (ev.action != input::KeyAction::PRESS) return;

        if (ev.modifiers & input::MOD_SHIFT) {
            if (ev.keycode == KeyCode::PAGE_UP) {
                keyboard_focus_tty->term->scrollback_up(keyboard_focus_tty->term->visible_rows() / 4);
                return;
            }
            if (ev.keycode == KeyCode::PAGE_DOWN) {
                keyboard_focus_tty->term->scrollback_down(keyboard_focus_tty->term->visible_rows() / 4);
                return;
            }
        }

        if (!keyboard_focus_tty->term->is_at_bottom()) {
            keyboard_focus_tty->term->scrollback_to_bottom();
        }

        if (!keyboard_focus_tty->canonical) {
            struct SpecialKey {
                KeyCode key;
                const char* seq;
                usize len;
            };
            static constexpr SpecialKey TABLE[] = {
                {KeyCode::ARROW_UP,    "\033[A",   3},
                {KeyCode::ARROW_DOWN,  "\033[B",   3},
                {KeyCode::ARROW_RIGHT, "\033[C",   3},
                {KeyCode::ARROW_LEFT,  "\033[D",   3},
                {KeyCode::HOME,        "\033[H",   3},
                {KeyCode::END,         "\033[F",   3},
                {KeyCode::INSERT,      "\033[2~",  4},
                {KeyCode::DELETE,      "\033[3~",  4},
                {KeyCode::PAGE_UP,     "\033[5~",  4},
                {KeyCode::PAGE_DOWN,   "\033[6~",  4},
                {KeyCode::F1,          "\033OP",   3},
                {KeyCode::F2,          "\033OQ",   3},
                {KeyCode::F3,          "\033OR",   3},
                {KeyCode::F4,          "\033OS",   3},
                {KeyCode::F5,          "\033[15~", 5},
                {KeyCode::F6,          "\033[17~", 5},
                {KeyCode::F7,          "\033[18~", 5},
                {KeyCode::F8,          "\033[19~", 5},
                {KeyCode::F9,          "\033[20~", 5},
                {KeyCode::F10,         "\033[21~", 5},
                {KeyCode::F11,         "\033[23~", 5},
                {KeyCode::F12,         "\033[24~", 5},
            };
            for (const auto& sk : TABLE) {
                if (ev.keycode == sk.key) {
                    for (usize i = 0; i < sk.len && keyboard_focus_tty->raw_len < TTY::BUFFER_SIZE - 1; i++)
                        keyboard_focus_tty->raw_buffer[keyboard_focus_tty->raw_len++] = sk.seq[i];
                    return;
                }
            }
        }

        const char c = ev.ascii;
        if (!c) return;  // If we input CTRL+Space (NULL) this gets swallowed here.

        if (c == '\b') {
            if (keyboard_focus_tty->canonical) {
                if (keyboard_focus_tty->canon_len > 0 && !keyboard_focus_tty->line_ready) {
                    keyboard_focus_tty->canon_len--;
                    keyboard_focus_tty->term->clear_char();
                }
                return;
            }
        }

        if (c == 3 && keyboard_focus_tty->canonical) {
            if (const RealmId fg = keyboard_focus_tty->fg_realm_id; fg != 0) {
                if (const Realm* realm = RealmManager::get(fg)) {
                    Unit* u = realm->unit_list;
                    while (u) {
                        signal_send(u, Signal::SIGINT);
                        u = u->realm_next;
                    }
                }
            }
            return;
        }

        if (keyboard_focus_tty->canonical) {
            // Canonical Mode
            if (c == '\n') {
                if (!keyboard_focus_tty->line_ready && keyboard_focus_tty->canon_len < TTY::BUFFER_SIZE - 1) {
                    keyboard_focus_tty->canon_buffer[keyboard_focus_tty->canon_len++] = '\n';
                    keyboard_focus_tty->canon_buffer[keyboard_focus_tty->canon_len] = '\0';
                    keyboard_focus_tty->line_ready = true;
                }
            } else {
                if (!keyboard_focus_tty->line_ready && keyboard_focus_tty->canon_len < TTY::BUFFER_SIZE - 1) {
                    keyboard_focus_tty->canon_buffer[keyboard_focus_tty->canon_len++] = c;
                    keyboard_focus_tty->term->put_char_fast(c);
                }
            }
        } else {
            // Non-canonical Mode
            if (keyboard_focus_tty->raw_len < TTY::BUFFER_SIZE - 1) {
                keyboard_focus_tty->raw_buffer[keyboard_focus_tty->raw_len++] = c;
            }
        }
    }

    // reference: https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797
    static u64 ansi_to_colour(const int code, bool is_bg, const bool bright = false) {
        switch (code) {
            case 30:
            case 40:
                return bright ? 0x00808080 : BLACK;  // gray for bright black
            case 31:
            case 41:
                return bright ? 0x00FF6060 : RED;
            case 32:
            case 42:
                return bright ? 0x0060FF60 : GREEN;
            case 33:
            case 43:
                return bright ? 0x00FFFF60 : YELLOW;
            case 34:
            case 44:
                return bright ? 0x0060A0FF : BLUE;
            case 35:
            case 45:
                return bright ? 0x00FF60FF : MAGENTA;
            case 36:
            case 46:
                return bright ? 0x0060FFFF : CYAN;
            case 37:
            case 47:
                return bright ? 0x00FFFFFF : WHITE;

            case 90:
            case 100:
                return 0x00808080;  // Bright Black (→ Gray)
            case 91:
            case 101:
                return 0x00FF6060;  // Bright Red
            case 92:
            case 102:
                return 0x0060FF60;  // Bright Green
            case 93:
            case 103:
                return 0x00FFFF60;  // Bright Yellow
            case 94:
            case 104:
                return 0x0060A0FF;  // Bright Blue
            case 95:
            case 105:
                return 0x00FF60FF;  // Bright Magenta
            case 96:
            case 106:
                return 0x0060FFFF;  // Bright Cyan
            case 97:
            case 107:
                return 0x00FFFFFF;  // Bright White

            default:
                return WHITE;  // fallback
        }
    }

    static void tty_apply_sgr(TTY* tty) {
        usize i = 0;
        while (i < tty->esc_param_count) {
            switch (const int code = tty->esc_params[i++]) {
                case 0:  // Reset
                    tty->fg = WHITE;
                    tty->bg = BLACK;
                    tty->reverse = false;
                    break;

                case 7:  // Reverse video
                    tty->reverse = true;
                    break;

                case 27:  // Reset reverse
                    tty->reverse = false;
                    break;

                case 30 ... 37:  // Standard FG
                    tty->fg = ansi_to_colour(code, false);
                    break;
                case 40 ... 47:  // Standard BG
                    tty->bg = ansi_to_colour(code, true);
                    break;

                case 90 ... 97:  // Bright FG
                    tty->fg = ansi_to_colour(code, false, true);
                    break;
                case 100 ... 107:  // Bright BG
                    tty->bg = ansi_to_colour(code, true, true);
                    break;

                case 38:  // Extended FG
                    if (i < tty->esc_param_count && tty->esc_params[i] == 2 && i + 3 < tty->esc_param_count) {
                        const int r = tty->esc_params[i + 1];
                        const int g = tty->esc_params[i + 2];
                        const int b = tty->esc_params[i + 3];
                        tty->fg = ((r << 16) | (g << 8) | b);
                        i += 4;
                    }
                    break;

                case 39:  // Reset FG to default
                    tty->fg = WHITE;
                    break;

                case 48:  // Extended BG
                    if (i < tty->esc_param_count && tty->esc_params[i] == 2 && i + 3 < tty->esc_param_count) {
                        const int r = tty->esc_params[i + 1];
                        const int g = tty->esc_params[i + 2];
                        const int b = tty->esc_params[i + 3];
                        tty->bg = ((r << 16) | (g << 8) | b);
                        i += 4;
                    }
                    break;

                case 49:  // Reset BG to default
                    tty->bg = BLACK;
                    break;

                default:
                    break;
            }
        }

        u32 fg = tty->fg;
        u32 bg = tty->bg;

        if (tty->reverse) {
            u32 tmp = fg;
            fg = bg;
            bg = tmp;
        }

        tty->term->set_colour(fg, bg);
    }

    void tty_process_output(TTY* tty, const char c) {
        switch (tty->esc_state) {
            case EscapeState::NONE:
                if (c == 0x1B) {
                    tty->term->flush();
                    tty->esc_state = EscapeState::ESC_RECEIVED;
                } else if (c == '\n') {
                    tty->term->flush();
                    tty->term->new_line();
                    tty->cursor_x = 0;
                    tty->cursor_y++;
                } else if (c == '\r') {
                    tty->term->flush();
                    tty->cursor_x = 0;
                    tty->term->set_cursor(0, tty->cursor_y);
                } else {
                    uint32_t cp = 0;

                    if (utf8_decode(&tty->utf8, static_cast<uint8_t>(c), &cp)) {
                        tty->term->put_codepoint(cp);
                        tty->cursor_x++;
                    }
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
                if (c == '?') {
                    tty->esc_private_mode = true;
                    break;
                }
                if (c >= '0' && c <= '9') {
                    tty->esc_param = tty->esc_param * 10 + (c - '0');
                    break;
                }
                if (c == ';') {
                    if (tty->esc_param_count < TTY::MAX_PARAMS)
                        tty->esc_params[tty->esc_param_count++] = tty->esc_param;
                    tty->esc_param = 0;
                    break;
                }
                if (tty->esc_param_count < TTY::MAX_PARAMS) tty->esc_params[tty->esc_param_count++] = tty->esc_param;

                {
                    // Helper: return params[idx], defaulting to `def` when
                    // the parameter was omitted (ANSI default = 0 stored).
                    auto p = [&](const usize idx, const int def) -> int {
                        if (idx >= tty->esc_param_count) return def;
                        const int v = tty->esc_params[idx];
                        return v == 0 ? def : v;
                    };

                    const usize cols = tty->term->visible_cols();
                    const usize rows = tty->term->visible_rows();

                    if (tty->esc_private_mode) {
                        // DEC private sequences
                        const int mode = (tty->esc_param_count > 0) ? tty->esc_params[0] : 0;
                        switch (c) {
                            case 'h':
                                if (mode == 25) tty->term->set_cursor_visible(true);
                                break;
                            case 'l':
                                if (mode == 25) tty->term->set_cursor_visible(false);
                                break;
                            default:
                                break;
                        }
                    } else {
                        // CSI sequences
                        switch (c) {
                            // SGR
                            case 'm':
                                tty_apply_sgr(tty);
                                break;
                            case 'H':  // ESC[row;colH  — cursor position (1-indexed)
                            case 'f': {
                                int row = p(0, 1) - 1;
                                int col = p(1, 1) - 1;
                                if (row < 0) row = 0;
                                if (col < 0) col = 0;
                                if (static_cast<usize>(row) >= rows) row = static_cast<int>(rows) - 1;
                                if (static_cast<usize>(col) >= cols) col = static_cast<int>(cols) - 1;
                                tty->cursor_y = row;
                                tty->cursor_x = col;
                                tty->term->set_cursor(col, row);
                                break;
                            }

                            case 'A': {  // ESC[nA — cursor up
                                const int n = p(0, 1);
                                int r = static_cast<int>(tty->cursor_y) - n;
                                if (r < 0) r = 0;
                                tty->cursor_y = r;
                                tty->term->set_cursor(tty->cursor_x, tty->cursor_y);
                                break;
                            }

                            case 'B': {  // ESC[nB — cursor down
                                usize r = tty->cursor_y + p(0, 1);
                                if (r >= rows) r = rows - 1;
                                tty->cursor_y = r;
                                tty->term->set_cursor(tty->cursor_x, tty->cursor_y);
                                break;
                            }

                            case 'C': {  // ESC[nC — cursor forward (right)
                                usize col = tty->cursor_x + p(0, 1);
                                if (col >= cols) col = cols - 1;
                                tty->cursor_x = col;
                                tty->term->set_cursor(tty->cursor_x, tty->cursor_y);
                                break;
                            }

                            case 'D': {  // ESC[nD — cursor back (left)
                                int col = static_cast<int>(tty->cursor_x) - p(0, 1);
                                if (col < 0) col = 0;
                                tty->cursor_x = col;
                                tty->term->set_cursor(tty->cursor_x, tty->cursor_y);
                                break;
                            }

                            case 'E': {  // ESC[nE — cursor next line
                                usize r = tty->cursor_y + p(0, 1);
                                if (r >= rows) r = rows - 1;
                                tty->cursor_y = r;
                                tty->cursor_x = 0;
                                tty->term->set_cursor(0, tty->cursor_y);
                                break;
                            }
                            case 'F': {  // ESC[nF — cursor previous line
                                int r = static_cast<int>(tty->cursor_y) - p(0, 1);
                                if (r < 0) r = 0;
                                tty->cursor_y = r;
                                tty->cursor_x = 0;
                                tty->term->set_cursor(0, tty->cursor_y);
                                break;
                            }
                            case 'G': {  // ESC[nG — cursor horizontal absolute (1-indexed)
                                int col = p(0, 1) - 1;
                                if (col < 0) col = 0;
                                if (static_cast<usize>(col) >= cols) col = static_cast<int>(cols) - 1;
                                tty->cursor_x = col;
                                tty->term->set_cursor(tty->cursor_x, tty->cursor_y);
                                break;
                            }
                            case 'd': {  // ESC[nd — line position absolute (1-indexed)
                                int row = p(0, 1) - 1;
                                if (row < 0) row = 0;
                                if (static_cast<usize>(row) >= rows) row = static_cast<int>(rows) - 1;
                                tty->cursor_y = row;
                                tty->term->set_cursor(tty->cursor_x, tty->cursor_y);
                                break;
                            }
                            case 's':
                                tty->saved_cursor_x = tty->cursor_x;
                                tty->saved_cursor_y = tty->cursor_y;
                                break;

                            case 'u':
                                tty->cursor_x = tty->saved_cursor_x;
                                tty->cursor_y = tty->saved_cursor_y;
                                tty->term->set_cursor(tty->cursor_x, tty->cursor_y);
                                break;
                            case 'J': {  // ESC[nJ — erase in display
                                const int mode = (tty->esc_param_count > 0) ? tty->esc_params[0] : 0;
                                if (mode == 2) {
                                    tty_clear(tty);
                                } else {
                                    tty->term->erase_in_display(mode, tty->cursor_x, tty->cursor_y);
                                    tty->term->flush();
                                }
                                break;
                            }

                            case 'K': {  // ESC[nK — erase in line
                                const int mode = (tty->esc_param_count > 0) ? tty->esc_params[0] : 0;
                                tty->term->erase_in_line(mode, tty->cursor_x, tty->cursor_y);
                                tty->term->flush();
                                break;
                            }
                            case 'n': {  // ESC[6n - Cursor Position Report
                                if (const int mode = p(0, 0); mode == 6) {
                                    // ESC [ row ; col R  (1-indexed)
                                    char response[32];
                                    const int len = snprintf(
                                        response,
                                        sizeof(response),
                                        "\033[%zu;%zuR",
                                        tty->cursor_y + 1,
                                        tty->cursor_x + 1
                                    );

                                    for (int i = 0; i < len && tty->raw_len < TTY::BUFFER_SIZE - 1; ++i) {
                                        tty->raw_buffer[tty->raw_len++] = response[i];
                                    }

                                    if (tty->canonical) {
                                        for (int i = 0; i < len && tty->canon_len < TTY::BUFFER_SIZE - 1; ++i)
                                            tty->canon_buffer[tty->canon_len++] = response[i];
                                        tty->canon_buffer[tty->canon_len] = '\0';
                                        tty->line_ready = true;
                                    }
                                }
                                break;
                            }

                            default:
                                break;  // If we don't know the sequence, we don't process it lol
                        }
                    }
                }

                // Reset CSI state.
                tty->esc_state = EscapeState::NONE;
                tty->esc_param = 0;
                tty->esc_param_count = 0;
                tty->esc_private_mode = false;
                break;
        }
    }

    void tty_clear(TTY* tty) {
        tty->term->clear();

        tty->canon_len = 0;
        tty->line_ready = false;
        memset(tty->canon_buffer, 0, TTY::BUFFER_SIZE);

        tty->cursor_x = tty->cursor_y = 0;

        tty->esc_state = EscapeState::NONE;
        tty->esc_param = 0;
        tty->esc_param_count = 0;
    }

    isize tty_read(TTY* tty, char* buf, const usize count) {
        const Unit* u = kernel::scheduling::get_current_unit();
        const RealmId my_realm = u ? u->rid : 0;

        usize bytes_read = 0;

        if (tty->canonical) {
            while (!tty->line_ready || (tty->fg_realm_id != 0 && tty->fg_realm_id != my_realm)) {
                if (u && (u->signals_pending & ~u->signals_masked)) {
                    return -EINTR;
                }
                kernel::scheduling::yield();
            }

            const usize to_copy = (tty->canon_len < count) ? tty->canon_len : count;
            memcpy(buf, tty->canon_buffer, to_copy);
            bytes_read = to_copy;

            tty->canon_len = 0;
            tty->line_ready = false;
            memset(tty->canon_buffer, 0, TTY::BUFFER_SIZE);
        } else {
            while (tty->raw_len == 0 || (tty->fg_realm_id != 0 && tty->fg_realm_id != my_realm)) {
                if (u && (u->signals_pending & ~u->signals_masked)) {
                    return -EINTR;
                }
                kernel::scheduling::yield();
            }

            const usize to_copy = (tty->raw_len < count) ? tty->raw_len : count;
            memcpy(buf, tty->raw_buffer, to_copy);
            bytes_read = to_copy;

            memmove(tty->raw_buffer, tty->raw_buffer + to_copy, tty->raw_len - to_copy);
            tty->raw_len -= to_copy;
        }

        return bytes_read;
    }
}  // namespace kernel::tty
