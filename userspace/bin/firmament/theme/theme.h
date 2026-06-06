// theme.h
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

#ifndef VESPERAOS_THEME_H
#define VESPERAOS_THEME_H

#include <stella.h>

/* -------------------------------------------------------------------------
 * Colour palette: 0xRRGGBB
 * ------------------------------------------------------------------------- */

/* Backgrounds */
#define VESPERA_BG 0x1e1e2eu     /* Main background                   */
#define VESPERA_BAR 0x13131fu    /* Topbar / panel background         */
#define VESPERA_BORDER 0x2a2a40u /* Dividers and outlines             */

/* Accent blues */
#define VESPERA_BLUE 0x4275f5u       /* Primary accent (vivid)            */
#define VESPERA_BLUE_MUTED 0x6495edu /* Text-weight blue                  */

/* Text */
#define VESPERA_TEXT 0xcdd6f4u     /* Primary text (Lavender)           */
#define VESPERA_TEXT_DIM 0x7a8ab0u /* Secondary / dim text              */

/* Dusk tones */
#define VESPERA_DUSK_RED 0xc04a6au   /* Dark rose-red                     */
#define VESPERA_DUSK_EMBER 0xe07050u /* Warm orange-red                   */
#define VESPERA_DUSK_WINE 0x8b2a45u  /* Deep wine                         */

/* Semantic */
#define VESPERA_GREEN 0x4ade80u /* OK / connected                    */
#define VESPERA_WARN 0xf0a030u  /* Warning                           */

/**
 * Convert a palette token to a stella_color_t.
 *
 *   stella_widget_set_bg(w, VESPERA_COL(VESPERA_BG), STELLA_OPA_COVER);
 */
#define VESPERA_COL(hex) stella_hex(hex)

/**
 * Apply the VesperaOS theme to a window.
 * Call once after stella_window_create(), before building any widgets.
 */
void vespera_theme_init(stella_window_t *win);

#endif  // VESPERAOS_THEME_H
