// testapp.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 31.05.26.
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

// test_app.c
// Minimale Stella-Testanwendung für VesperaOS.
// Öffnet ein 800×600-Fenster, zeigt Text an und reagiert
// auf den Compositor-Close-Button.

#include <stella.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// ---------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------

static long long now_ms(void) {
    timespec_t ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static void sleep_ms(long long ms) {
    if (ms <= 0) return;
    timespec_t ts = {
        .tv_sec  = ms / 1000LL,
        .tv_nsec = (ms % 1000LL) * 1000000LL,
    };
    nanosleep(&ts, NULL);
}

// ---------------------------------------------------------------------------
// Close-Callback
// ---------------------------------------------------------------------------

static void on_close(stella_window_t *win, void *user_data) {
    (void)win;
    (void)user_data;
    printf("test_app: WINDOW_CLOSED signal empfangen\n");
    // Hier könnten z.B. Dateien gespeichert, Netzwerkverbindungen
    // getrennt oder andere Ressourcen freigegeben werden.
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    stella_init();

    // --- Fenster anlegen ---
    stella_config_t cfg = {
        .width  = 800,
        .height = 600,
        .title  = "Stella Testfenster",
        .flags  = 0,
    };

    stella_window_t *win = stella_window_create(&cfg);
    if (!win) {
        printf("test_app: stella_window_create fehlgeschlagen\n");
        return 1;
    }

    // --- UI aufbauen ---
    stella_widget_t screen = stella_window_get_screen(win);

    // Hintergrundfarbe (dunkles Blau wie der Compositor-Default)
    stella_widget_set_bg(screen, (stella_color_t){._raw = 0x1A1A2E}, 255);

    // Zentrierter Haupttext
    stella_widget_t lbl_main = stella_label_create(screen, "Hallo von Stella!");
    stella_text_set_font(lbl_main, STELLA_FONT_24);
    stella_text_set_color(lbl_main, (stella_color_t){._raw = 0xD0D0E8});
    stella_widget_center(lbl_main);

    // Kleiner Hinweistext darunter
    stella_widget_t lbl_hint = stella_label_create(screen, "Fenster ueber den Close-Button schliessen.");
    stella_text_set_font(lbl_hint, STELLA_FONT_12);
    stella_text_set_color(lbl_hint, (stella_color_t){._raw = 0x888899});
    stella_widget_align(lbl_hint, STELLA_ALIGN_BOTTOM_MID, 0, -24);

    // --- Close-Callback registrieren ---
    stella_window_on_close(win, on_close, NULL);

    // --- Hauptschleife (~60 fps) ---
    const long long FRAME_MS = 16LL;
    long long last = now_ms();

    while (!stella_window_should_close(win)) {
        stella_process_events(win);

        long long now   = now_ms();
        uint32_t  delta = (uint32_t)(now - last);
        last = now;

        if (delta > 0)
            stella_tick(delta);

        // Restzeit bis zum nächsten Frame schlafen
        long long elapsed = now_ms() - now;
        sleep_ms(FRAME_MS - elapsed);
    }

    // --- Aufräumen ---
    printf("test_app: Beende sauber.\n");
    stella_window_destroy(win);
    return 0;
}