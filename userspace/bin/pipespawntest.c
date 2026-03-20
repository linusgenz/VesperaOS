// pipespawntest.c
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
#include <sysstd.h>
#include <realm.h>
#include <vespera/spawn.h>

int main(void) {
    int64_t fds[2];
    if (sys_pipe((uint64_t)fds, 0, 0, 0, 0, 0) < 0) {
        printf("pipe failed\n");
        return 1;
    }

    // Writer spawnen: stdout → fds[1]
    spawn_config_t wcfg = { .stdin_handle = 0, .stdout_handle = fds[1], .stderr_handle = 0 };
    const char* wargv[] = { "/bin/pipewriter", "hello from writer\n", NULL };
    int64_t wrid = spawn_realm("/bin/pipewriter", (char**)wargv, NULL, &wcfg);

    // Reader spawnen: stdin → fds[0]
    spawn_config_t rcfg = { .stdin_handle = fds[0], .stdout_handle = 0, .stderr_handle = 0 };
    const char* rargv[] = { "/bin/pipereader", NULL };
    int64_t rrid = spawn_realm("/bin/pipereader", (char**)rargv, NULL, &rcfg);

    // Shell-seitige Enden schließen
    sys_close(fds[0], 0, 0, 0, 0, 0);
    sys_close(fds[1], 0, 0, 0, 0, 0);

    // Warten
    int status = 0;
    wait_realm(wrid, &status);
    wait_realm(rrid, &status);

    printf("pipe spawn test done\n");
    return 0;
}