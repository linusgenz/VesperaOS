// log_writer.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 10.10.25.
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

#include "log_writer.h"
#include "../../filesystem/vfs/vfs.h"

FileLogWriter::FileLogWriter(const char *p) : file_handle(nullptr), path(p) {
    file_handle = vfs_open(path);
    if (!file_handle) {
        vfs_create(path);
        file_handle = vfs_open(path);
    }
}

FileLogWriter::~FileLogWriter() {
    if (file_handle) {
        vfs_close(file_handle);
    }
}

bool FileLogWriter::append_line(const char *line, size_t len) {
    if (!file_handle) return false;
    ssize_t w = file_handle->ops->write(file_handle, file_handle->size, len, line);
    return (w == (ssize_t) len);
}
