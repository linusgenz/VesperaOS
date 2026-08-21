// termios.h
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
#ifndef _TERMIOS_H
#define _TERMIOS_H

#include <stdint.h>
#include <stdio.h>
#include <vespera/dev/ioctl_tty.h>
#include <sys/ioctl.h>

/**
 * @brief Read the current mode of a TTY device.
 *
 * Queries the TTY associated with @p tty and fills @p t with its current
 * mode and echo settings.
 *
 * @param tty  File handle referring to the TTY device (usually @c stdin).
 * @param t    Pointer to a @c tty_mode_t struct to be filled on success.
 * @return     @c 0 on success, negative error code on failure.
 *
 * @see tcsetattr()
 * @see tty_mode_t
 */
static inline int tcgetattr(FILE_HANDLE tty, tty_mode_t* t) {
    return (int)ioctl(tty, IOCTL_TTY_GET_MODE, t);
}

/**
 * @brief Apply a mode change to a TTY device.
 *
 * Sets the operating mode and echo behavior of the TTY associated with
 * @p tty. Switching between @c TTY_MODE_CANONICAL and @c TTY_MODE_RAW
 * drains any pending input so the new mode starts clean.
 *
 * @param tty  File handle referring to the TTY device (usually @c stdin).
 * @param t    Pointer to a @c tty_mode_t struct describing the desired mode.
 * @return     @c 0 on success, negative error code on failure.
 *
 * @see tcgetattr()
 * @see tty_mode_t
 * @see TTY_MODE_CANONICAL
 * @see TTY_MODE_RAW
 */
static inline int tcsetattr(FILE_HANDLE tty, tty_mode_t* t) {
    return (int)ioctl(tty, IOCTL_TTY_SET_MODE, t);
}

/**
 * @brief Query the dimensions of a TTY device.
 *
 * Fills @p s with the current number of rows and columns visible in the
 * terminal. The values reflect the physical character grid of the framebuffer
 * terminal and do not change at runtime unless the font is switched.
 *
 * @param tty  File handle referring to the TTY device (usually @c stdin).
 * @param s    Pointer to a @c tty_size_t struct to be filled with row and
 *             column counts on success.
 * @return     @c 0 on success, negative error code on failure.
 *
 * @see tty_size_t
 */
static inline int tty_get_size(FILE_HANDLE tty, tty_size_t* s) {
    return (int)ioctl(tty, IOCTL_TTY_GET_SIZE, s);
}

#endif  // VESPERAOS_TERMIOS_H
