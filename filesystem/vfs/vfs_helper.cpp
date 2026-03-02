// vfs_helper.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 02.08.25.
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

#include "vfs_node.h"
#include "../../include/string.h"
#include "../../include/path.h"
#include "vfs.h"

bool VFS::resolve_parent(const char* path, VfsNode** parent_out, char* name_out)
{
    if (!path || !parent_out || !name_out) return false;

    char components[16][32];
    size_t count = split_path(path, components, 16);
    if (count == 0) return false;

    if (count == 1)
    {
        *parent_out = open("/"); // root dir
        strncpy(name_out, components[0], 31);
        name_out[31] = '\0';
        return *parent_out != nullptr;
    }

    // Reconstruct parent path, e.g. "/mnt/sd0/foo/bar" → parent: "/mnt/sd0/foo", name: "bar"
    char parent_path[256] = {};
    parent_path[0] = '/';

    for (size_t i = 0; i < count - 1; i++)
    {
        strncat(parent_path, components[i], sizeof(parent_path) - strlen(parent_path) - 1);
        if (i < count - 2)
            strncat(parent_path, "/", sizeof(parent_path) - strlen(parent_path) - 1);
    }

    *parent_out = open(parent_path);
    if (!*parent_out) return false;

    strncpy(name_out, components[count - 1], 31);
    name_out[31] = '\0';
    return true;
}

dirent_type_t VFS::node_type_to_dirent_type(VfsNodeType type)
{
    switch (type)
    {
    case VfsNodeType::File: return DT_FILE;
    case VfsNodeType::Directory: return DT_DIR;
    case VfsNodeType::CharDevice: return DT_CHARDEV;
    case VfsNodeType::BlockDevice: return DT_BLOCKDEV;
    case VfsNodeType::OtherDevice:
    default:
        return DT_UNKNOWN;
    }
}

void VFS::ensure_path_exists(const char* path)
{
    if (!path || path[0] != '/') return;

    char temp[256];
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char components[16][32];
    size_t count = split_path(temp, components, 16);

    char current[256] = "/";
    for (size_t i = 0; i < count; i++)
    {
        if (strlen(current) > 1) strcat(current, "/");
        strcat(current, components[i]);

        if (const VfsNode* node = VFS::open(current); !node)
        {
            VFS::mkdir(current);
        }
    }
}
