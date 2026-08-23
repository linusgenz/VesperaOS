// vespera_gl_offscreen_test.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 22.08.26.
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

#include <GLES2/gl2.h>
#include <stdio.h>

#include "../vespera-gl-offscreen/vespera_gl_offscreen.h"

int main(void) {
    struct vespera_gl_offscreen ctx;

    printf("vespera_gl_offscreen_test: creating context...\n");
    if (!vespera_gl_offscreen_create(&ctx, 256, 256)) {
        fprintf(stderr, "vespera_gl_offscreen_test: context creation failed\n");
        return 1;
    }
    printf("vespera_gl_offscreen_test: context created, GL is current\n");

    /* Einfachster moeglicher Test: reiner Clear, kein Draw-Call, keine
     * Shader-Kompilierung. Wenn das schon durchlaeuft, ist die gesamte
     * Kette bis zum Backbuffer nachweislich funktionsfaehig. */
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        fprintf(stderr, "vespera_gl_offscreen_test: glGetError() = 0x%x\n", err);
    }

    printf("vespera_gl_offscreen_test: writing PPM...\n");
    if (!vespera_gl_offscreen_write_ppm(&ctx, "/tmp/vespera_gl_test.ppm")) {
        fprintf(stderr, "vespera_gl_offscreen_test: PPM write failed\n");
        vespera_gl_offscreen_destroy(&ctx);
        return 1;
    }

    printf("vespera_gl_offscreen_test: OK -- see /tmp/vespera_gl_test.ppm\n");

    vespera_gl_offscreen_destroy(&ctx);
    return 0;
}