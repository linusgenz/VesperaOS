// desktop.h
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
#ifndef VESPERAOS_DESKTOP_H
#define VESPERAOS_DESKTOP_H

#include <stdbool.h>
#include <stdint.h>
#include <stella.h>

#define DESKTOP_TOPBAR_HEIGHT 28
#define DESKTOP_TASKBAR_HEIGHT 36
#define DESKTOP_ICON_GRID_WIDTH 72 /* Width of the left icon column       */
#define DESKTOP_ICON_SIZE 40       /* Width + height of a single icon box */
#define DESKTOP_ICON_MAX 16        /* Maximum number of desktop icons     */

typedef struct {
    const char *label;         /* Display name shown below the icon       */
    const char *icon_src;      /* const char* filesystem path (e.g. "/usr/share/icons/myapp.png") or NULL */
    stella_color_t icon_color; /* Fallback icon background colour         */
    void (*on_launch)(void *user_data);
    void *launch_data;
} desktop_icon_t;

/**
 * Build the complete desktop layer on the given screen widget.
 * Call AFTER topbar_create() / taskbar_create() so the content area is
 * positioned correctly.
 */
void desktop_create(stella_widget_t screen);

/**
 * Return the content area widget (between topbar and taskbar).
 * App windows are placed here as children.
 * Returns NULL if desktop_create() has not yet been called.
 */
stella_widget_t desktop_get_content_area(void);

/**
 * Add an icon to the left icon column.
 * Returns false when DESKTOP_ICON_MAX is reached.
 */
bool desktop_add_icon(const desktop_icon_t *icon);

/** Remove all icons and rebuild the grid. */
void desktop_clear_icons(void);

#endif  // VESPERAOS_DESKTOP_H
