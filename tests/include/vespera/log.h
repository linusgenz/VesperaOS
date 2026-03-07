// log.h
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
#pragma once
#define LOG_H
#include <cstdio>
#include <cstdarg>

namespace Log {
    void error(const char* fmt, ...);
    void debug(const char* fmt, ...);
    void warning(const char* fmt, ...);
    void info(const char* fmt, ...);
    void ok(const char* fmt, ...);
    void print(const char* fmt, ...);
    void print_ln(const char* fmt, ...);
}