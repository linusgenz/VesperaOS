// file_backing.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 12.08.26.
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
#ifndef VESPERAOS_VFS_BACKING_H
#define VESPERAOS_VFS_BACKING_H

#include <filesystem/vfs_node.h>
#include <vespera/mm/vm_backing.h>
#include <vespera/sync/spinlock.h>
#include <vespera/types.h>

class FileBackingObject final : public kernel::vm::VmBackingObject {
public:
    explicit FileBackingObject(VfsNode* node);
    ~FileBackingObject() override;

    /**
     * @brief Returns the FileBackingObject shared by all mmap()s of @p node, creating it
     *        if this is the first mapping.
     *
     * @return Existing or newly-created backing object, or nullptr on allocation failure
     *         or if @p node is null.
     */
    static FileBackingObject* get_or_create(VfsNode* node);

    phys_addr_t get_page(usize offset_in_bytes) override;
    void sync_page(usize offset_in_bytes, phys_addr_t phys, bool is_dirty) override;
    void add_mapping() override;
    void remove_mapping() override;
    usize get_size() const override;

private:
    VfsNode* node_{nullptr};
    usize size_{0u};

    const MountPoint* mount_{nullptr};
    u64 inode_id_{0u};


    // Page Caching
    phys_addr_t* pages_{nullptr};
    usize page_count_{0u};

    u32 mapping_count_{0u};
    mutable Spinlock lock_{"file_backing_lock"};

    void check_and_destroy() const;
};

#endif //VESPERAOS_VFS_BACKING_H
