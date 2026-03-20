// pipetest.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 20.03.26.
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sysstd.h>
#include <vespera/spawn.h>

int main(int argc, const char** argv) {
    // Pipe anlegen
    int64_t fds[2];
    int64_t rc = sys_pipe((uint64_t)fds, 0, 0, 0, 0, 0);
    if (rc < 0) {
        printf("pipe() failed: %lld\n", rc);
        return 1;
    }
    printf("pipe ok: read=%p write=%p\n", fds[0], fds[1]);

    // In die Write-End schreiben
    const char* msg = "hello from pipe\n";
    ssize_t w = sys_write(fds[1], (uint64_t)msg, strlen(msg), 0, 0, 0);
    printf("wrote %lld bytes\n", w);

    // Aus der Read-End lesen
    char buf[64] = {0};
    ssize_t r = sys_read(fds[0], (uint64_t)buf, sizeof(buf) - 1, 0, 0, 0);
    printf("read %lld bytes: %s", r, buf);

    sys_close(fds[0], 0, 0, 0, 0, 0);
    sys_close(fds[1], 0, 0, 0, 0, 0);
    return 0;
}