// statusbar.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 02.04.26.
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
#ifndef VESPERAOS_STATUSBAR_H
#define VESPERAOS_STATUSBAR_H

#include <stddef.h>

// Maximum length of visible characters in a status message
#define STATUSBAR_MSG_MAX 128

void statusbar_init(void);

// Redraw the status bar in-place (cursor save/restore)
void statusbar_draw(void);

// for system events e.g. new device mounted
void statusbar_set_message(const char* msg);

#endif  // VESPERAOS_STATUSBAR_H
