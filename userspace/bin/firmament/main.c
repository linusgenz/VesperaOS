// main.c
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

#include <realm.h>
#include <stdio.h>
#include <stella.h>
#include <time.h>

#include "vbus.h"
#include "crepusculum_protocol.h"
#include "shell/desktop.h"
#include "shell/taskbar.h"
#include "shell/topbar.h"
#include "theme/theme.h"

static void start_app_button_cb(stella_widget_t widget, void *user_data) {
    (void)widget;
    (void)user_data;

    const char *app_argv[] = {"/bin/testapp", NULL};
    const char *app_envp[] = {"PATH=/bin", NULL};

    int64_t rid = spawn_realm("/bin/testapp", (char **)app_argv, (char **)app_envp, NULL);
    if (rid < 0) {
        printf("Desktop: Fehler beim Starten von testapp (%lld)\n", rid);
    } else {
        printf("Desktop: testapp erfolgreich gestartet! Realm: %lld\n", rid);
    }
}

static void on_taskbar_entry_click(void *data) {
    uint32_t wid = (uint32_t)(uintptr_t)data;

    vbus_display_window_id_t payload = {.window_id = wid};

    vbus_signal(VBUS_IFACE_DISPLAY, VBUS_DISP_WINDOW_ACTIVATE, get_realm_id(), &payload, sizeof(payload));
}

static void firmament_process_global_events(const vbus_header_t *hdr, const vbus_payload_t *payload) {
    if (strcmp(hdr->member, VBUS_DISP_WINDOW_OPENED) == 0) {
        void *click_data = (void *)(uintptr_t)payload->opened.window_id;

        taskbar_window_entry_t entry = {
            .id = payload->opened.window_id,
            .title = payload->opened.title,
            .icon_color = VESPERA_COL(VESPERA_BLUE),
            .icon_src = NULL,
            .on_click = on_taskbar_entry_click,
            .click_data = click_data,
        };
        taskbar_add_window(&entry);


    } else if (strcmp(hdr->member, VBUS_DISP_WINDOW_CLOSED) == 0) {
        taskbar_remove_window(payload->closed.window_id);

    } else if (strcmp(hdr->member, VBUS_DISP_WINDOW_MINIMIZED) == 0) {
        // Setzt den Button optisch auf "minimiert" (z.B. ausgegraut)
        taskbar_set_minimized(payload->minimized.window_id, true);
        // HINWEIS: Kein taskbar_clear_focused() mehr hier!
        // Der Server schickt gleich danach eh ein WINDOW_FOCUSED mit ID 0 oder der neuen ID.

    } else if (strcmp(hdr->member, VBUS_DISP_WINDOW_RESTORED) == 0) {
        // Setzt den Button optisch wieder auf den Normalzustand
        taskbar_set_minimized(payload->restored.window_id, false);

    } else if (strcmp(hdr->member, VBUS_DISP_WINDOW_FOCUSED) == 0) {
        // Das ist das zentrale Event für das optische Highlighten!
        uint32_t new_focused_id = payload->focused.window_id;

        if (new_focused_id == 0) {
            // Keine App hat mehr Fokus (z.B. Desktop angeklickt oder alles minimiert)
            taskbar_clear_focused();
        } else {
            // Setzt das optische Highlight (z.B. blauer Rahmen/Hintergrund) auf das neue Fenster.
            // Deine taskbar_set_focused() Funktion sollte intern den alten Fokus automatisch löschen.
            taskbar_set_focused(new_focused_id);
        }
    }
}

int main(void) {
    if (stella_init() < 0) {
        return 1;
    }

    stella_config_t firmament_conf = {
        .width = 0,
        .height = 0,
        .flags = VBUS_DISP_FLAG_FULLSCREEN,
        .title = "Firmament Desktop",
    };

    stella_window_t *desktop_win = stella_window_create(&firmament_conf);
    if (!desktop_win) {
        return 1;
    }

    vespera_theme_init(desktop_win);

    stella_widget_t scr = stella_window_get_screen(desktop_win);

    desktop_create(scr);
    topbar_create(scr);
    taskbar_create(scr);

    RealmID rid = get_realm_id();

    vbus_display_set_strut_t strut = {
        .edge = CREP_STRUT_TOP,
        .size = TOPBAR_H,
    };
    vbus_signal(VBUS_IFACE_DISPLAY, VBUS_DISP_SET_STRUT, rid, &strut, sizeof(strut));

    vbus_display_set_strut_t strut_bot = {
        .edge = CREP_STRUT_BOTTOM,
        .size = TASKBAR_H,
    };
    vbus_signal(VBUS_IFACE_DISPLAY, VBUS_DISP_SET_STRUT, rid, &strut_bot, sizeof(strut));

    stella_widget_t btn = stella_button_create(scr, "Klick mich", 200, 50);
    stella_widget_align(btn, STELLA_ALIGN_CENTER, 0, 0);
    stella_widget_on_click(btn, start_app_button_cb, NULL);

    vbus_header_t hdr;
    vbus_payload_t payload;

    while (1) {
        while (vbus_recv(&hdr, &payload, sizeof(payload)) == 1) {
            stella_handle_event(desktop_win, &hdr, &payload);

            firmament_process_global_events(&hdr, &payload);
        }

        stella_tick(5);
        usleep(5000);
    }

    stella_window_destroy(desktop_win);
    return 0;
}
