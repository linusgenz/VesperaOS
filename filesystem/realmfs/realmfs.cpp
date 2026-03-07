/**
 * @file realmfs.cpp
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

#include "realmfs.h"

#include <vespera/realm/realm_manager.h>
#include <vespera_errno.h>

#include "../../kernel/units/unit_manager.h"
#include <klib/string.h>
#include <vespera/mm/memory.h>

void RealmFs::init()
{
    VirtualFilesystem::init("/realms", "realms");

    // Setup operations
    ops_.read = read;
    ops_.write = write;
    ops_.find = find;
    ops_.close = close;
    ops_.opendir = open_dir;
    ops_.readdir = read_dir;
    ops_.closedir = close_dir;
    ops_.ioctl = ioctl;
    ops_.create = nullptr;
    ops_.rename = nullptr;
    ops_.mkdir = nullptr;
    ops_.rmdir = nullptr;
    ops_.unlink = nullptr;
}


int RealmFs::register_realm(uint64_t realm_id, const char* name, void* realm_ptr)
{
    SpinlockGuard guard(lock_);


    VfsNode* realm_dir = ensure_subdirectory(name, root_);
    ensure_subdirectory("units", realm_dir);

    auto* obj = static_cast<SysObject*>(kernel::memory::malloc(sizeof(SysObject)));
    strncpy(obj->name, name, sizeof(obj->name) - 1);
    obj->name[sizeof(obj->name) - 1] = '\0';
    obj->type = SYS_OBJ_REALM;
    obj->id = realm_id;
    obj->manager_ref = realm_ptr;

    create_entry_node("info", realm_dir, obj, VfsNodeType::CharDevice);

    return SUCCESS_CODE;
}

int RealmFs::register_unit(uint64_t unit_id, const char* name, void* unit_ptr, const char* realm_name)
{
    SpinlockGuard guard(lock_);

    VfsNode* realm_dir = find(root_, realm_name);
    if (!realm_dir) return -ENOENT;

    VfsNode* units_dir = finddir(realm_dir, "units");
    if (!units_dir) return -EFAULT;

    auto* obj = static_cast<SysObject*>(kernel::memory::malloc(sizeof(SysObject)));
    strncpy(obj->name, name, sizeof(obj->name) - 1);
    obj->name[63] = '\0';
    obj->type = SYS_OBJ_UNIT;
    obj->id = unit_id;
    obj->manager_ref = unit_ptr;

    auto* entry = create_entry_node(obj->name, units_dir, obj, VfsNodeType::CharDevice);
    auto* unit_entry = entry;
    unit_entry->obj_type = SYS_OBJ_UNIT;
    unit_entry->parent_realm = realm_dir;

    return SUCCESS_CODE;
}

int RealmFs::unregister_realm(uint64_t realm_id)
{
    SpinlockGuard guard(lock_);

    auto* root_data = static_cast<DirData*>(root_->internal_data);

    for (size_t i = 0; i < root_data->subdirs.size(); i++)
    {
        VfsNode* realm_dir = root_data->subdirs[i];

        auto* dir_data = static_cast<DirData*>(realm_dir->internal_data);
        RealmFsEntry* realm_entry = nullptr;

        for (auto* file_node : dir_data->files)
        {
            if (auto* entry = static_cast<RealmFsEntry*>(file_node->internal_data);
                entry && entry->device && entry->device->id == realm_id)
            {
                realm_entry = entry;
                break;
            }
        }

        if (!realm_entry) continue;

        delete_entry_node(realm_dir);
        root_data->subdirs.erase(i);

        return SUCCESS_CODE;
    }
    return -ENOENT;
}

int RealmFs::unregister_unit(uint64_t unit_id)
{
    SpinlockGuard guard(lock_);

    for (auto* root_data = static_cast<DirData*>(root_->internal_data); const auto* realm_dir : root_data->subdirs)
    {
        VfsNode* units_dir = finddir(realm_dir, "units");
        if (!units_dir) continue;

        auto* units_data = static_cast<DirData*>(units_dir->internal_data);
        for (size_t i = 0; i < units_data->files.size(); i++)
        {
            VfsNode* u_node = units_data->files[i];
            if (const auto* u_entry = static_cast<RealmFsEntry*>(u_node->internal_data);
                u_entry && u_entry->device->id == unit_id)
            {
                units_data->files.erase(i);
                delete_entry_node(u_node);
                return SUCCESS_CODE;
            }
        }
    }

    return -ENOENT;
}

ssize_t RealmFs::read(const VfsNode* node, size_t offset, size_t size, void* buffer)
{
    if (!node) return -EINVAL;

    auto* entry = static_cast<RealmFsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    if (const SysObject* obj = entry->device; obj->type == SYS_OBJ_REALM)
    {
        return RealmManager::get_status(obj->manager_ref, buffer, size, offset);
    }
    else if (obj->type == SYS_OBJ_UNIT)
    {
        return UnitManager::get_status(obj->manager_ref, buffer, size, offset);
    }

    return -ENFILE;
}

ssize_t RealmFs::write(VfsNode* node, size_t offset, size_t size, const void* buffer)
{
    if (!node) return -EINVAL;

    auto* entry = static_cast<RealmFsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    if (const SysObject* obj = entry->device; obj->type == SYS_OBJ_REALM)
    {
        //      return RealmManager::control(obj->manager_ref, buffer, size);
    }
    else if (obj->type == SYS_OBJ_UNIT)
    {
        //      return UnitManager::control(obj->manager_ref, buffer, size);
    }

    return -EUNSUPPORTED;
}

ssize_t RealmFs::ioctl(const VfsNode* node, uint32_t cmd, void* arg)
{
    if (!node) return -EINVAL;

    auto* entry = static_cast<RealmFsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    SysObject* obj = entry->device;

    // ioctl for advanced operations
    /*  if (obj->type == SYS_OBJ_REALM) {
          switch (cmd) {
              case REALM_IOCTL_ENTER:
                  return RealmManager::enter_realm(obj->manager_ref);
              case REALM_IOCTL_LEAVE:
                  return RealmManager::leave_realm(obj->manager_ref);
              case REALM_IOCTL_ADD_UNIT:
                  return RealmManager::add_unit(obj->manager_ref, arg);
              case REALM_IOCTL_GET_CAPS:
                  return RealmManager::get_capabilities(obj->manager_ref, arg);
              case REALM_IOCTL_LIST_UNITS:
                  return RealmManager::list_units(obj->manager_ref, arg);
              default:
                  return -EINVAL;
          }
      } else if (obj->type == SYS_OBJ_UNIT) {
          switch (cmd) {
              case UNIT_IOCTL_START:
                  return UnitManager::start_unit(obj->manager_ref);
              case UNIT_IOCTL_STOP:
                  return UnitManager::stop_unit(obj->manager_ref);
              case UNIT_IOCTL_RESTART:
                  return UnitManager::restart_unit(obj->manager_ref);
              case UNIT_IOCTL_GET_STATUS:
                  return UnitManager::get_unit_status(obj->manager_ref, arg);
              case UNIT_IOCTL_SET_CONFIG:
                  return UnitManager::set_config(obj->manager_ref, arg);
              default:
                  return -EINVAL;
          }
      }*/

    return -EUNSUPPORTED;
}

void RealmFs::close(VfsNode* node)
{
    // Cleanup if needed in the future
}
