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

#include <vespera/filesystem/vfs.h>

FileLogWriter::FileLogWriter(const char* file_path)
    : file_handle_(nullptr)
    , path_(file_path) {
    auto res = VFS::open(path_);
    if (res.is_err()) {
        VFS::create(path_);
        res = VFS::open(path_);
    }
    if (res.is_ok()) file_handle_ = res.unwrap();
}

FileLogWriter::~FileLogWriter() {
    if (file_handle_) VFS::close(file_handle_);
}

bool FileLogWriter::append_line(const char* line, const usize len) {
    if (!file_handle_) return false;
    auto res = file_handle_->ops->write(file_handle_, file_handle_->size, len, line);
    return res.is_ok() && res.unwrap() == len;
}