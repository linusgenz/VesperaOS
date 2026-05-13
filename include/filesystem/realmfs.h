/**
 * @file RealmFS.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 08.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef VESPERAOS_REALM_FS_H
#define VESPERAOS_REALM_FS_H

#include "filesystem/virtual_fs.h"

// System object types
enum SysObjectType
{
    SYS_OBJ_REALM,
    SYS_OBJ_UNIT,
};

// Generic system object
struct SysObject
{
    char name[64];
    SysObjectType type;
    u64 id;
    void* manager_ref; // Pointer to realm/unit in manager
};

// Extended entry with system-specific data
struct RealmFsEntry : VirtualFsEntry<SysObject>
{
    SysObjectType obj_type{};
    VfsNode* units_dir{nullptr};
    VfsNode* parent_realm{nullptr};
};

class RealmFs : public VirtualFilesystem<SysObject, RealmFsEntry>
{
public:
    static void init();

    static int register_realm(u64 realm_id, const char* name, void* realm_ptr);
    static VoidResult register_unit(u64 unit_id, const char* name, void* unit_ptr, const char* realm_name);

    static int unregister_realm(u64 realm_id);
    static VoidResult unregister_unit(u64 unit_id);

    // VFS operations
    static Result<usize> read(const VfsNode* node, usize offset, usize size, void* buffer);
    static Result<usize> write(VfsNode* node, usize offset, usize size, const void* buffer);
    static isize ioctl(const VfsNode* node, u32 cmd, void* arg);
    static void close(VfsNode* node);
};

// ioctl commands for realms and units
#define REALM_IOCTL_ENTER      0x1001
#define REALM_IOCTL_LEAVE      0x1002
#define REALM_IOCTL_ADD_UNIT   0x1003
#define REALM_IOCTL_GET_CAPS   0x1004
#define REALM_IOCTL_LIST_UNITS 0x1005

#define UNIT_IOCTL_START       0x2001
#define UNIT_IOCTL_STOP        0x2002
#define UNIT_IOCTL_RESTART     0x2003
#define UNIT_IOCTL_GET_STATUS  0x2004
#define UNIT_IOCTL_SET_CONFIG  0x2005

#endif //VESPERAOS_RealmFS_H
