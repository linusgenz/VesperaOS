// urandom.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 26.09.25.
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

#ifndef VESPERAOS_URANDOM_H
#define VESPERAOS_URANDOM_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * @brief Read random bytes from /dev/urandom into the provided buffer.
 *
 * This function attempts to open `/dev/urandom` and fill `buf` with `buflen`
 * random bytes. The read is performed in a loop to tolerate partial reads
 * until the buffer is full or an error occurs.
 *
 * @param buf Pointer to the destination buffer to fill with random data.
 * @param buflen Number of bytes to read into the buffer.
 * @return On success: number of bytes actually read (should equal `buflen`).
 *         On failure: negative error code (-EINVAL, -ENOENT, -EIO).
 */
ssize_t getrandom(void *buf, size_t buflen);

/**
 * @brief Generate a 32-bit random unsigned integer.
 *
 * This function reads 4 random bytes from `/dev/urandom` and returns them
 * as a 32-bit value. In case of an error, the negative error code is returned
 * as the function result.
 *
 * @return On success: a random 32-bit integer.
 *         On failure: a negative error code (e.g., -ENOENT, -EIO).
 */
int32_t urandom_u32(void);

/**
 * @brief Generate a 64-bit random unsigned integer.
 *
 * This function reads 8 random bytes from `/dev/urandom` and returns them
 * as a 64-bit value. In case of an error, the negative error code is returned
 * as the function result.
 *
 * @return On success: a random 64-bit integer.
 *         On failure: a negative error code (e.g., -ENOENT, -EIO).
 */
int64_t urandom_u64(void);


#endif //VESPERAOS_URANDOM_H