// dirstream.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 22.08.26.
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

#ifndef VESPLIB_INTERNAL_DIRSTREAM_H
#define VESPLIB_INTERNAL_DIRSTREAM_H

#include <vespera/dirent.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Backing storage for a POSIX DIR* stream.
 *
 * `path` is retained so rewinddir() can re-open the stream at the same
 * location (see rewinddir()'s doc comment in dirent.h for why: there is
 * currently no native rewind/seek primitive in sys_readdir()).
 */
struct __dirstream {
    int fd;              ///< fd-table slot for the open directory handle
    dirent_t entry;      ///< storage for the entry last returned by readdir()
    int at_eof;           ///< 1 once sys_readdir() has reported end-of-directory
    char* path;           ///< duplicated path used to open this stream, or NULL
                           ///< if the stream was created via fdopendir() (in which
                           ///< case rewinddir() cannot re-open it)
};

#ifdef __cplusplus
}
#endif

#endif  // VESPLIB_INTERNAL_DIRSTREAM_H
