// xcursor_loader.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 29.05.26.
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
#ifndef VESPERAOS_XCURSOR_LOADER_H
#define VESPERAOS_XCURSOR_LOADER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t* pixels;
    uint32_t width;
    uint32_t height;
    uint32_t xhot;
    uint32_t yhot;
} loaded_cursor_t;

bool xcursor_load_file(const char* filename, uint32_t target_size, loaded_cursor_t* out_cursor);

void xcursor_free(loaded_cursor_t* cursor);

#endif  // VESPERAOS_XCURSOR_LOADER_H
