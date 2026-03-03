// log.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 02.03.26.
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

#include "log.h"

#include <cstdarg>
#include <cstdio>

static void vprint(const char* fmt, std::va_list args) {
    vprintf(fmt, args);
}

#define DEFINE_LOG_FN(name)                \
    void Log::name(const char* fmt, ...) { \
        va_list args;                      \
        va_start(args, fmt);               \
        vprint(fmt, args);                 \
        va_end(args);                      \
        printf("\n");                      \
    }

DEFINE_LOG_FN(Info)
DEFINE_LOG_FN(Ok)
DEFINE_LOG_FN(Warning)
DEFINE_LOG_FN(Error)
DEFINE_LOG_FN(debug)
DEFINE_LOG_FN(Print)
DEFINE_LOG_FN(PrintLn)