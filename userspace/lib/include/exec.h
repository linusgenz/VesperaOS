// exec.h
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

#ifndef VESPERAOS_EXEC_H
#define VESPERAOS_EXEC_H

/**
 * @brief Find the absolute path of an executable using PATH.
 *
 * @param name Program name (e.g., "lsusb").
 * @return Pointer to a static buffer containing the path,
 *         or NULL if not found.
 */
const char *find_executable(const char *name, char **envp);


#endif //VESPERAOS_EXEC_H
