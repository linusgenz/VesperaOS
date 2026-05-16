// vfs_handle.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.05.26.
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

#include <filesystem/vfs_node.h>
#include <filesystem/vfs_handle.h>

VfsHandle::~VfsHandle() {
    if (node) {
        if (node->type == VfsNodeType::Directory &&
            context && context->type_specific_data) {
            VFS::closedir(context->type_specific_data);
            context->type_specific_data = nullptr;
            }

        VFS::close(node);
    }
    delete context;
}