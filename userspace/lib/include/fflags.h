// fflags.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 22.09.25.
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

#ifndef VESPERAOS_FCNTL_H
#define VESPERAOS_FCNTL_H

#define O_RDONLY    0x0000  /**< Open for reading only */
#define O_WRONLY    0x0001  /**< Open for writing only */
#define O_RDWR      0x0002  /**< Open for reading and writing */

#define O_CREAT     0x0040  /**< Create file if it does not exist */
#define O_EXCL      0x0080  /**< Exclusive use flag */
#define O_TRUNC     0x0200  /**< Truncate file to zero length */
#define O_APPEND    0x0400  /**< Append mode */

#define SEEK_SET    0  /**< Seek relative to start of file */
#define SEEK_CUR    1  /**< Seek relative to current position */
#define SEEK_END    2  /**< Seek relative to end of file */

#endif //VESPERAOS_FCNTL_H