// FileDescriptor.cpp
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

#include "FileDescriptor.h"
#include "cstdint"

namespace kernel {
    int64_t alloc_fd(VfsNode* node) {
        for (int i = 0; i < 256; ++i) {
            if (!kernel::fd_table[i].used) {
                kernel::fd_table[i].node = node;
                kernel::fd_table[i].offset = 0;
                kernel::fd_table[i].used = true;
                return i;
            }
        }
        return -1;
    }

    FileDescriptor* get_fd(int64_t fd) {
        if (fd < 0 || fd >= 256 || !kernel::fd_table[fd].used) return nullptr;
        return &kernel::fd_table[fd];
    }

    void free_fd(int64_t fd) {
        if (fd < 0 || fd >= MAX_FDS) return;

        FileDescriptor& desc = fd_table[fd];
        if (!desc.used) return;

        if (desc.node && desc.node->ops && desc.node->ops->close) {
            desc.node->ops->close(desc.node);
        }

        desc.used = 0;
        desc.node = nullptr;
        desc.offset = 0;
    }

}