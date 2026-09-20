// intel_gem_backing.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 09.09.26.
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

#include "intel_gem_backing.h"

#include "intel_luc_file.h"

namespace gpu::intel::core {

    IntelGemBackingObject::IntelGemBackingObject(const LucFile* file, const u32 handle)
        : obj_(file->gem_get_ref(handle)) {
    }

    IntelGemBackingObject::~IntelGemBackingObject() {
        if (obj_) {
            obj_->dec_ref();
        }
    }

    phys_addr_t IntelGemBackingObject::get_page(const usize offset_in_bytes) {
        if (!obj_ || obj_->is_userptr) {
            return phys_addr_t{};
        }

        if (offset_in_bytes >= obj_->size) {
            return phys_addr_t{};
        }

        // GEM_CREATE's backing is contiguous → no page table walk
        return phys_add(obj_->phys_addr, offset_in_bytes);
    }

    usize IntelGemBackingObject::get_size() const {
        return obj_ ? obj_->size : 0;
    }

} // namespace gpu::intel::core
