// log_writer.h
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

#ifndef VESPERAOS_LOG_WRITER_H
#define VESPERAOS_LOG_WRITER_H

#include <vespera/system/system_manager.h>

#include <filesystem/vfs.h>

class FileLogWriter final : public kernel::ILogWriter {
   public:
    explicit FileLogWriter(const char* file_path);
    ~FileLogWriter() override;

    bool append_line(const char* line, usize len) override;

   private:
    VfsNode* file_handle_;
    const char* path_;
};

#endif  // VESPERAOS_LOG_WRITER_H
