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

#include <filesystem/vfs.h>
#include <klib/path.h>
#include <klib/string.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>

#include <filesystem/vfs_node.h>
#include "../kernel/realm/realm.h"
#include "vespera/log.h"

bool VFS::resolve_parent(const char* path, VfsNode** parent_out, char* name_out) {
    if (!path || !parent_out || !name_out) return false;

    char components[16][32];
    const usize count = split_path(path, components, 16);
    if (count == 0) return false;

    if (count == 1) {
        *parent_out = open("/").value_or(nullptr); // root dir
        strncpy(name_out, components[0], 31);
        name_out[31] = '\0';
        return *parent_out != nullptr;
    }

    // Reconstruct parent path, e.g. "/mnt/sd0/foo/bar" → parent: "/mnt/sd0/foo", name: "bar"
    char parent_path[256] = {};
    parent_path[0] = '/';

    for (usize i = 0; i < count - 1; i++) {
        strncat(parent_path, components[i], sizeof(parent_path) - strlen(parent_path) - 1);
        if (i < count - 2)
            strncat(parent_path, "/", sizeof(parent_path) - strlen(parent_path) - 1);
    }

    *parent_out = open(parent_path).value_or(nullptr);;
    if (!*parent_out) return false;

    strncpy(name_out, components[count - 1], 31);
    name_out[31] = '\0';
    return true;
}

dirent_type_t VFS::node_type_to_dirent_type(const VfsNodeType type) {
    switch (type) {
        case VfsNodeType::File: return DT_REG;
        case VfsNodeType::Directory: return DT_DIR;
        case VfsNodeType::CharDevice: return DT_CHR;
        case VfsNodeType::BlockDevice: return DT_BLK;
        case VfsNodeType::Fifo: return DT_FIFO;
        case VfsNodeType::Symlink: return DT_LNK;
        case VfsNodeType::Socket: return DT_SOCK;
        case VfsNodeType::OtherDevice:
        default:
            return DT_UNKNOWN;
    }
}

void VFS::ensure_path_exists(const char* path) {
    if (!path || path[0] != '/') return;

    char temp[256];
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char components[16][32];
    const usize count = split_path(temp, components, 16);

    char current[256] = "/";
    for (usize i = 0; i < count; i++) {
        if (strlen(current) > 1) strcat(current, "/");
        strcat(current, components[i]);

        if (const VfsNode* node = VFS::open(current).value_or(nullptr); !node) {
            VFS::mkdir(current, 0755);
        }
    }
}

VoidResult VFS::resolve_path(const char* user_path, char* out, usize out_size) {
    if (!user_path || user_path[0] == '\0' || !out || out_size == 0)
        return VoidResult::err(Error::Inval);

    Realm* realm = kernel::scheduling::get_current_realm();
    if (!realm) return VoidResult::err(Error::Srch);

    const char* root = (realm->root_path[0] != '\0') ? realm->root_path : "/";
    const char* cwd = (realm->cwd_path[0] != '\0') ? realm->cwd_path : root;

    char abs[256];
    if (user_path[0] != '/') {
        if (strcmp(cwd, "/") == 0)
            snprintf(abs, sizeof(abs), "/%s", user_path);
        else
            snprintf(abs, sizeof(abs), "%s/%s", cwd, user_path);
    } else {
        if (strcmp(root, "/") == 0) {
            strncpy(abs, user_path, sizeof(abs) - 1);
            abs[sizeof(abs) - 1] = '\0';
        } else {
            snprintf(abs, sizeof(abs), "%s%s", root, user_path);
        }
    }

    char normalized[256];
    normalize_path(abs, normalized, sizeof(normalized));

    usize root_len = strlen(root);
    char root_cmp[256];
    strncpy(root_cmp, root, sizeof(root_cmp) - 1);
    root_cmp[sizeof(root_cmp) - 1] = '\0';
    if (root_len > 1 && root_cmp[root_len - 1] == '/')
        root_cmp[--root_len] = '\0';

    if (strcmp(root_cmp, "/") != 0) {
        if (strncmp(normalized, root_cmp, root_len) != 0 ||
            (normalized[root_len] != '/' && normalized[root_len] != '\0')) {
            return VoidResult::err(Error::Acces);
        }
    }

    strncpy(out, normalized, out_size - 1);
    out[out_size - 1] = '\0';
    return VoidResult::ok();
}

VoidResult VFS::resolve_path_at(const char* base_path, const char* user_path, char* out, usize out_size) {
    if (!user_path || user_path[0] == '\0' || !out || out_size == 0)
        return VoidResult::err(Error::Inval);
    Realm* realm = kernel::scheduling::get_current_realm();
    if (!realm) return VoidResult::err(Error::Srch);
    const char* root = (realm->root_path[0] != '\0') ? realm->root_path : "/";

    // base_path stands in for cwd_path here.
    const char* base = (base_path && base_path[0] != '\0') ? base_path : root;

    char abs[256];
    if (user_path[0] != '/') {
        if (strcmp(base, "/") == 0)
            snprintf(abs, sizeof(abs), "/%s", user_path);
        else
            snprintf(abs, sizeof(abs), "%s/%s", base, user_path);
    } else {
        if (strcmp(root, "/") == 0) {
            strncpy(abs, user_path, sizeof(abs) - 1);
            abs[sizeof(abs) - 1] = '\0';
        } else {
            snprintf(abs, sizeof(abs), "%s%s", root, user_path);
        }
    }
    char normalized[256];
    normalize_path(abs, normalized, sizeof(normalized));
    usize root_len = strlen(root);
    char root_cmp[256];
    strncpy(root_cmp, root, sizeof(root_cmp) - 1);
    root_cmp[sizeof(root_cmp) - 1] = '\0';
    if (root_len > 1 && root_cmp[root_len - 1] == '/')
        root_cmp[--root_len] = '\0';
    if (strcmp(root_cmp, "/") != 0) {
        if (strncmp(normalized, root_cmp, root_len) != 0 ||
            (normalized[root_len] != '/' && normalized[root_len] != '\0')) {
            return VoidResult::err(Error::Acces);
            }
    }
    strncpy(out, normalized, out_size - 1);
    out[out_size - 1] = '\0';
    return VoidResult::ok();
}
