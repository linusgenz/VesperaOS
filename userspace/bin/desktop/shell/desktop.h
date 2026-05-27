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

#include <lvgl/lvgl.h>
#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * Layout-Konstanten — müssen mit topbar.c / taskbar.c übereinstimmen
 * ------------------------------------------------------------------------- */
#define DESKTOP_TOPBAR_HEIGHT    28
#define DESKTOP_TASKBAR_HEIGHT   36
#define DESKTOP_ICON_GRID_WIDTH  72   /* Breite der linken Icon-Spalte       */
#define DESKTOP_ICON_SIZE        40   /* Breite + Höhe einer Icon-Box        */
#define DESKTOP_ICON_MAX         16   /* Maximale Anzahl Desktop-Icons       */

/* -------------------------------------------------------------------------
 * Deskop-Icon Descriptor
 *
 * on_launch wird aufgerufen wenn der Nutzer ein Icon anklickt.
 * icon_src:  lv_image_dsc_t* für ein echtes Bild, oder NULL für
 *            den Fallback (farbige Box mit erstem Buchstaben des Labels).
 * ------------------------------------------------------------------------- */
typedef struct {
    const char       *label;       /* Angezeigter Name unter dem Icon       */
    const void       *icon_src;    /* lv_image_dsc_t* oder NULL             */
    lv_color_t        icon_color;  /* Hintergrundfarbe des Fallback-Icons   */
    void            (*on_launch)(void);
} desktop_icon_t;

/* -------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

/**
 * Baut den kompletten Desktop-Layer auf dem übergebenen Screen auf.
 * Muss NACH topbar_create() und taskbar_create() aufgerufen werden,
 * damit die Content-Area korrekt positioniert wird.
 */
void desktop_create(lv_obj_t *screen);

/**
 * Gibt den Content-Bereich zurück (zwischen Topbar und Taskbar).
 * Hier werden später App-Fenster platziert.
 * NULL wenn desktop_create() noch nicht aufgerufen wurde.
 */
lv_obj_t *desktop_get_content_area(void);

/* -------------------------------------------------------------------------
 * Icon-Management
 * ------------------------------------------------------------------------- */

/**
 * Fügt ein Icon zur linken Icon-Leiste hinzu.
 * Gibt false zurück wenn DESKTOP_ICON_MAX erreicht ist.
 */
bool desktop_add_icon(const desktop_icon_t *icon);

/**
 * Entfernt alle Icons und baut das Grid neu auf.
 */
void desktop_clear_icons(void);


#endif  // VESPERAOS_DESKTOP_H
