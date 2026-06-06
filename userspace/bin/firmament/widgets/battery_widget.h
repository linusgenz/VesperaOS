// battery_widget.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 27.05.26.
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
#ifndef VESPERAOS_BATTERY_WIDGET_H
#define VESPERAOS_BATTERY_WIDGET_H

#include <stella.h>

typedef struct battery_ctx battery_ctx_t;

stella_widget_t battery_widget_create(stella_widget_t parent, int battery_index);

void battery_widget_destroy(battery_ctx_t *ctx);

#endif  // VESPERAOS_BATTERY_WIDGET_H
