// tty.h
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

#ifndef VESPERAOS_TTY_H
#define VESPERAOS_TTY_H

#include <kernel/input/input_event.h>

class TTYDevice;

namespace kernel::tty {
    enum class EscapeState {
        NONE,
        ESC_RECEIVED,
        CSI_RECEIVED
    };

    struct TTY {
        static constexpr size_t BUFFER_SIZE = 1024;
        static constexpr size_t MAX_PARAMS = 16;

        // Canonical
        char canon_buffer[BUFFER_SIZE];
        size_t canon_len = 0;
        bool line_ready = false;

        // Non-canonical
        char raw_buffer[BUFFER_SIZE];
        size_t raw_len = 0;

        bool canonical = false;

        EscapeState esc_state = EscapeState::NONE;
        int esc_param = 0;
        int esc_params[MAX_PARAMS] = {};
        int esc_param_count = 0;

        size_t cursor_x = 0;
        size_t cursor_y = 0;

        Colour fg = WHITE;
        Colour bg = BLACK;
    };


    extern TTY tty_instances[6];
    extern TTYDevice *tty_devices[6];
    extern TTY *active_tty;

    void tty_init(TTY *tty);

    void tty_handle_input(const input::InputEvent &ev);

    void tty_process_output(TTY *tty, char c);

    void tty_clear(TTY *tty);

    size_t tty_read(char *buf, size_t count);
}

#endif //VESPERAOS_TTY_H
