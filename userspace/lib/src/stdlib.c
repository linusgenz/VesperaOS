// stdlib.c
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 23.09.25.
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

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define MAX_ENV_VARS 64

static char* environ[MAX_ENV_VARS];
static size_t env_count = 0;

char* getenv(const char* name, char** envp) {
    if (!name) return nullptr;
    size_t name_len = strlen(name);

    for (size_t i = 0; i < 1; i++) {
        if (strncmp(envp[i], name, name_len) == 0 && envp[i][name_len] == '=') {
            return envp[i] + name_len + 1;
        }
    }
    return nullptr;
}
/*
int setenv(const char* name, const char* value, int overwrite) {
    if (!name || !value) return -1;
    size_t name_len = strlen(name);


    for (size_t i = 0; i < env_count; i++) {
        if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=') {
            if (!overwrite) return 0;

            size_t new_len = name_len + 1 + strlen(value) + 1;
            char* new_entry = malloc(new_len);
            if (!new_entry) return -1;
            strcpy(new_entry, name);
            strcat(new_entry, "=");
            strcat(new_entry, value);

            environ[i] = new_entry;
            return 0;
        }
    }

    if (env_count >= MAX_ENV_VARS) return -1;
    size_t new_len = name_len + 1 + strlen(value) + 1;
    char* new_entry = malloc(new_len);
    if (!new_entry) return -1;
    strcpy(new_entry, name);
    strcat(new_entry, "=");
    strcat(new_entry, value);

    environ[env_count++] = new_entry;
    return 0;
}

int unsetenv(const char* name) {
    if (!name) return -1;
    size_t name_len = strlen(name);

    for (size_t i = 0; i < env_count; i++) {
        if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=') {
            // Löschen, Rest nach vorne schieben
            for (size_t j = i; j < env_count - 1; j++) {
                environ[j] = environ[j + 1];
            }
            env_count--;
            return 0;
        }
    }
    return -1;
}*/
