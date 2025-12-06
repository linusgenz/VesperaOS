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
#include <stdio.h>


char** environ = NULL;
size_t env_count = 0;
size_t env_capacity = 0;

FILE_HANDLE stdin;
FILE_HANDLE stdout;
FILE_HANDLE stderr;

void init_environ(char** envp)
{
    size_t count = 0;
    while (envp[count]) count++;

    env_capacity = count + 16; // more space for variables by default
    environ = malloc(env_capacity * sizeof(char*));
    if (!environ) return;

    for (size_t i = 0; i < count; i++)
    {
        size_t len = strlen(envp[i]) + 1;
        environ[i] = malloc(len);
        if (!environ[i]) return;
        memcpy(environ[i], envp[i], len);
    }

    environ[count] = NULL;
    env_count = count;
}

char* getenv(const char* name)
{
    if (!name || !environ) return NULL;
    size_t name_len = strlen(name);

    for (size_t i = 0; environ[i]; i++)
    {
        if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=')
        {
            return environ[i] + name_len + 1;
        }
    }
    return NULL;
}

int setenv(const char* name, const char* value, int overwrite)
{
    if (!name || !value || strchr(name, '=')) return -1;
    size_t name_len = strlen(name);

    for (size_t i = 0; i < env_count; i++)
    {
        if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=')
        {
            if (!overwrite) return 0;

            size_t new_len = name_len + 1 + strlen(value) + 1;
            char* new_entry = malloc(new_len);
            if (!new_entry) return -1;

            snprintf(new_entry, new_len, "%s=%s", name, value);

            free(environ[i]);
            environ[i] = new_entry;
            return 0;
        }
    }

    if (env_count + 1 >= env_capacity)
    {
        env_capacity = env_capacity * 2;
        char** new_environ = realloc(environ, env_capacity * sizeof(char*));
        if (!new_environ) return -1;
        environ = new_environ;
    }

    size_t new_len = name_len + 1 + strlen(value) + 1;
    char* new_entry = malloc(new_len);
    if (!new_entry) return -1;

    snprintf(new_entry, new_len, "%s=%s", name, value);

    environ[env_count] = new_entry;
    env_count++;
    environ[env_count] = NULL; // environ must be null-terminated

    return 0;
}

int unsetenv(const char* name)
{
    if (!name || strchr(name, '=')) return -1;
    size_t name_len = strlen(name);

    for (size_t i = 0; i < env_count; i++)
    {
        if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=')
        {
            free(environ[i]);

            for (size_t j = i; j < env_count - 1; j++)
            {
                environ[j] = environ[j + 1];
            }

            env_count--;
            environ[env_count] = NULL;

            return 0;
        }
    }
    return -1;
}
