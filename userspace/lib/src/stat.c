// stat.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.03.26.
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

#include <vespera/stat.h>
#include <sysstd.h>

#include <errno.h>

int stat(const char* path, struct stat* out) {
    return (int)sys_stat((uint64_t)path, (uint64_t)out, 0, 0, 0, 0);
}

int mkdir(const char* path, mode_t mode) {
    long ret = (int)sys_mkdir((uint64_t)path, (uint64_t)mode,0,0,0,0);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

int rmdir(const char* path) {
    int ret = (int)sys_rmdir((uint64_t)path, 0,0,0,0,0);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}


int is_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.v_node_type == VSTAT_TYPE_DIR;
}

int is_file(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.v_node_type == VSTAT_TYPE_FILE;
}