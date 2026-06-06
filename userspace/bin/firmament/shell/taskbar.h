// taskbar.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 03.06.26.
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

#ifndef VESPERAOS_SHELL_TASKBAR_H
#define VESPERAOS_SHELL_TASKBAR_H

#include <stdbool.h>
#include <stdint.h>
#include <stella.h>

/* -------------------------------------------------------------------------
 * Layout constants
 * ------------------------------------------------------------------------- */

#define TASKBAR_H          40    /* Total bar height (pixels)                */
#define TASKBAR_BTN_W     160    /* Width of one window button               */
#define TASKBAR_BTN_H      30    /* Height of one window button              */
#define TASKBAR_ICON_SZ    18    /* Small icon inside each window button     */
#define TASKBAR_MAX_WINS   24    /* Maximum number of tracked windows        */
#define TASKBAR_LAUNCHER_W 40    /* Width of the launcher button             */

/* -------------------------------------------------------------------------
 * Types
 * ------------------------------------------------------------------------- */

/**
 * Opaque handle identifying a window registered with the taskbar.
 * Returned by taskbar_add_window(); passed to all subsequent calls.
 */
typedef uint32_t taskbar_wid_t;

#define TASKBAR_WID_INVALID 0u

/**
 * Descriptor used when registering an open window with the taskbar.
 * The taskbar copies all string fields — callers do not need to keep them alive.
 */
typedef struct {
    uint32_t id;
    const char     *title;       /* Window title text (may be NULL)          */
    stella_color_t  icon_color;  /* Fallback icon background colour          */
    const char     *icon_src;    /* Path to an icon image file, or NULL      */
    void          (*on_click)(void *data); /* Called when the button is clicked */
    void           *click_data;  /* Passed back to on_click unmodified       */
} taskbar_window_entry_t;

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

/**
 * Create the taskbar and attach it to @screen.
 *
 * Call after topbar_create() and desktop_create().
 * The returned widget is owned by the taskbar; do not destroy it externally.
 */
stella_widget_t taskbar_create(stella_widget_t screen);

/**
 * Register @entry as an open window and add a button to the taskbar.
 *
 * Returns a handle for future taskbar_remove_window() / taskbar_set_focused()
 * calls.  Returns TASKBAR_WID_INVALID when TASKBAR_MAX_WINS is exceeded.
 */
taskbar_wid_t taskbar_add_window(const taskbar_window_entry_t *entry);

/**
 * Unregister window @id and remove its button from the taskbar.
 * Safe to call with TASKBAR_WID_INVALID (no-op).
 */
void taskbar_remove_window(taskbar_wid_t id);

/**
 * Highlight the button for @id as the active (focused) window.
 * Automatically clears any previously focused button.
 */
void taskbar_set_focused(taskbar_wid_t id);

/** Remove the focus highlight from every button. */
void taskbar_clear_focused(void);

/**
 * Update the title text displayed on a window's button.
 * Useful when a document title or window name changes at runtime.
 */
void taskbar_update_title(taskbar_wid_t id, const char *title);

/**
 * Register a callback that fires when the launcher button is clicked.
 * Typically used to open an app-grid or command launcher.
 * Pass NULL to disable the launcher button's action.
 */
void taskbar_set_launcher_cb(void (*cb)(void *data), void *data);

void taskbar_set_minimized(taskbar_wid_t id, bool minimized);

#endif // VESPERAOS_SHELL_TASKBAR_H
