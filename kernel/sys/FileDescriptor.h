// FileDescriptor.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 02.08.25.
//
// This file is part of LuminOS.
// 
// LuminOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// LuminOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with LuminOS. If not, see <https://www.gnu.org/licenses/>.

#ifndef FILEDESCRIPTOR_H
#define FILEDESCRIPTOR_H

#include "../../filesystem/vfs/vfs_node.h"
#include "cstdint"

struct FileDescriptor {
    VfsNode* node = nullptr;
    size_t offset = 0;
    bool used = false;
};

#define MAX_FDS 256

namespace kernel {
    inline FileDescriptor fd_table[MAX_FDS]; // max 256 open files TODO change this when we have processes

    int64_t alloc_fd(VfsNode* node);
    FileDescriptor* get_fd(int64_t fd);
    void free_fd(int64_t fd);
}

#endif //FILEDESCRIPTOR_H
