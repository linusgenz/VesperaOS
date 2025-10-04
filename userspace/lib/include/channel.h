// channel.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 01.10.25.
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

#ifndef VESPERAOS_CHANNEL_H
#define VESPERAOS_CHANNEL_H

#include <stdio.h>

/**
 * @brief Create a new channel with the specified capacity.
 *
 * This function wraps the low-level syscall to allocate a channel.
 * If capacity is 0, a default size (e.g., 4096 bytes) is used.
 *
 * @param capacity The desired capacity of the channel buffer in bytes.
 * @return On success, returns a positive CHANNEL_HANDLE representing the channel.
 *         On error, returns negative errno:
 *           -ENOMEM : insufficient memory to allocate channel
 *           -EINVAL : invalid current unit or realm
 */
CHANNEL_HANDLE channel_create(size_t capacity);

/**
 * @brief Receive data from a channel.
 *
 * Copies up to `len` bytes from the channel associated with the handle
 * into the provided user buffer.
 *
 * @param hid CHANNEL_HANDLE of the channel to receive from.
 * @param buf Pointer to a buffer where received data will be stored.
 * @param len Maximum number of bytes to receive.
 * @return On success, returns the number of bytes actually received.
 *         On error, returns negative errno:
 *           -EINVAL : invalid channel handle or resource
 *           -EBADH  : handle not found
 *           -EACCES : read capability missing
 *           -EAGAIN : channel is empty, try again later
 */
ssize_t channel_recv(CHANNEL_HANDLE hid, void* buf, size_t len);

/**
 * @brief Send data to a channel.
 *
 * Copies up to `len` bytes from the user buffer into the channel
 * associated with the handle.
 *
 * @param hid CHANNEL_HANDLE of the channel to send to.
 * @param data Pointer to the user buffer containing data to send.
 * @param len Number of bytes to send.
 * @return On success, returns the number of bytes actually sent.
 *         On error, returns negative errno:
 *           -EINVAL : invalid channel handle or resource
 *           -EBADH  : handle not found
 *           -EACCES : write capability missing
 *           -EAGAIN : channel is full, try again later
 */
ssize_t channel_send(CHANNEL_HANDLE hid, const void* data, size_t len);

#endif //VESPERAOS_CHANNEL_H