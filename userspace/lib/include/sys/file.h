// file.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.08.26.
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
#ifndef VESPERAWORKSPACE_FILE_H
#define VESPERAWORKSPACE_FILE_H

#include <stdbool.h>

/**
 * @brief Check if a file exists at the given path.
 *
 * @param path Path to check.
 * @return true if the file exists, false otherwise.
 */
bool file_exists(const char* path);

/* Operations for the `flock` call.  */
#define LOCK_SH 1   /* Shared lock.  */
#define LOCK_EX 2   /* Exclusive lock.  */
#define LOCK_UN 8   /* Unlock.  */

/* Can be OR'd in to one of the above.  */
#define LOCK_NB 4   /* Don't block when locking.  */

/* Apply or remove an advisory lock, according to OPERATION,
   on the file FD refers to.  */
int flock(int fd, int operation);

#endif //VESPERAWORKSPACE_FILE_H
