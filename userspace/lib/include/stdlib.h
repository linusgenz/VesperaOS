// stdlib.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 23.09.25.
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

#ifndef VESPERAOS_STDLIB_H
#define VESPERAOS_STDLIB_H

typedef long int ssize_t;

/**
 * @brief Get the value of an environment variable.
 *
 * @param name Variable name (null-terminated string).
 * @return Pointer to the value string, or NULL if not found.
 */
char* getenv(const char* name, char** envp);

/**
 * @brief Set an environment variable.
 *
 * @param name Variable name.
 * @param value Value to set.
 * @param overwrite If 0, existing variables are not overwritten.
 * @return 0 on success, -1 on failure.
 */
int setenv(const char* name, const char* value, int overwrite);

/**
 * @brief Unset an environment variable.
 *
 * @param name Variable name.
 * @return 0 on success, -1 if not found.
 */
int unsetenv(const char* name);

#endif //VESPERAOS_STDLIB_H