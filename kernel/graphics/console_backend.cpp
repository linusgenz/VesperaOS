// console_backend.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 20.09.25.
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
// You should have received a copy of the GNU General Public Licensed
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#include <basic_renderer.h>
#include <errno.h>
#include "console_backend.h"
#include "../types/types.h"
#include "../types/handle.h"


static ssize_t console_write(void* resource, const void* buf, size_t len) {
    (void)resource;
    if (!global_renderer) return -EIO;
    // global_renderer->print expects const char*,size_t
    global_renderer->print((const char*)buf, len);
    return (ssize_t)len;
}

static ssize_t console_read(void* resource, void* buf, size_t len) {
    (void)resource; (void)buf; (void)len;
    // no console input implemented here
    return 0;
}
/*
static void console_destroy(void* r) {
    file_backend_t* fb = (file_backend_t*) r;
    if (!fb) return;
    if (fb->impl) kernel::memory::free(fb->impl);
    kernel::memory::free(fb);
}

ErrorCode create_console_backend(file_backend_t** out_backend) {
    if (!out_backend) return MOD_ERR_INVALID_OPERATION;

    file_backend_t* backend = (file_backend_t*)kernel::memory::malloc(sizeof(file_backend_t));
    if (!backend) return MOD_ERR_OUT_OF_MEMORY;

    backend->write = console_write;
    backend->read  = console_read;
    backend->impl  = global_renderer; // dein globaler Renderer als Ressource

    *out_backend = backend;
    return MOD_SUCCESS;
}*/

