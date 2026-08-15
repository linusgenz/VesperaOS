// stat.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.03.26.
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
#ifndef VESPLIB_STAT_H
#define VESPLIB_STAT_H

#include <stdint.h>
#include <sysstd.h>
#include <vespera/stat.h>

int stat(const char *__restrict__ path, struct stat *__restrict__ buf);
int fstat(int fd, struct stat *buf);

/**
 * @brief Create a new directory.
 *
 * Creates a new directory at @p path.
 *
 * @param path Path where the directory should be created.
 * @return @c 0 on success, or @c -1 on failure (errno set).
 *
 * @see opendir()
 * @see creat()
 * @see rmdir()
 */
int mkdir(const char *pathname, mode_t mode);

/**
 * @brief Remove a directory.
 *
 * Removes (deletes) the empty directory at @p path.
 *
 * @param path Path to the directory to remove.
 * @return @c 0 on success, or @c -1 on failure (errno set).
 *
 * @see mkdir()
 * @see unlink()
 * @see remove()
 */
int rmdir(const char* path);

int is_directory(const char* path);

int is_file(const char* path);

#endif  // VESPLIB_STAT_H
