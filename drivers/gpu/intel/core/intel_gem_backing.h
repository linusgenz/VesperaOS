// intel_gem_backing.h
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

#ifndef VESPERAOS_INTEL_GEM_BACKING_H
#define VESPERAOS_INTEL_GEM_BACKING_H

#include <vespera/types.h>
#include <vespera/mm/addr.h>
#include <vespera/mm/vm_backing.h>

namespace gpu::intel::core {
    struct GemObject;

    /**
     * VmBackingObject adapter for mmap()'d GEM buffers.
     * Adopts a pre-acquired GemObject reference (see intel_gem_object.h) at
     * construction and releases it in the destructor. Lifetime of the backing
     * object itself is driven by VM mappings via add_mapping()/remove_mapping()
     * (atomic refs_); once the last mapping is torn down, remove_mapping()
     * deletes this object, which drops the GemObject ref. This keeps the
     * underlying pages alive even if GEM_CLOSE or LucFile destruction runs
     * while a mapping is still active.
     */
    class IntelGemBackingObject final : public kernel::vm::VmBackingObject {
    public:
        explicit IntelGemBackingObject(GemObject* adopted_obj);
        ~IntelGemBackingObject() override;

        IntelGemBackingObject(const IntelGemBackingObject&) = delete;
        IntelGemBackingObject& operator=(const IntelGemBackingObject&) = delete;

        phys_addr_t get_page(usize offset_in_bytes) override;
        [[nodiscard]] CacheMode get_cache_mode() const override;
        [[nodiscard]] usize get_size() const override;

        void add_mapping() override {
            __atomic_add_fetch(&refs_, 1, __ATOMIC_RELAXED);
        }

        void remove_mapping() override {
            if (__atomic_sub_fetch(&refs_, 1, __ATOMIC_ACQ_REL) == 0) {
                delete this;
            }
        }

    private:
        GemObject* obj_; ///< ref held for this object's lifetime
        u32 refs_ = 0;
    };
} // namespace gpu::intel::core

#endif // VESPERAOS_INTEL_GEM_BACKING_H
