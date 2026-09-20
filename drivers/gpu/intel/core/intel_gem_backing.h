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
    struct LucFile;
    struct GemObject;

    /**
     * Holds its own ref on the underlying GemObject (see
     * intel_luc_file.h), taken at construction and dropped in the
     * destructor. That ref is what keeps the object's pages alive if the
     * owning LucFile's GEM_CLOSE (or the whole file's release()) runs
     * while this mapping is still around -- add_mapping()/remove_mapping()
     * are still no-ops today (nothing here needs a *second* refcount on
     * top of the GemObject's own), but the constructor/destructor pair
     * now does real ref-holding instead of just remembering a handle.
     */
    class IntelGemBackingObject final : public kernel::vm::VmBackingObject {
    public:
        IntelGemBackingObject(const LucFile* file, u32 handle);
        ~IntelGemBackingObject() override;

        IntelGemBackingObject(const IntelGemBackingObject&) = delete;
        IntelGemBackingObject& operator=(const IntelGemBackingObject&) = delete;

        phys_addr_t get_page(usize offset_in_bytes) override;
        [[nodiscard]] usize get_size() const override;

        void add_mapping() override {}
        void remove_mapping() override {}

    private:
        GemObject* obj_; ///< ref held for this object's lifetime
    };
} // namespace gpu::intel::core

#endif // VESPERAOS_INTEL_GEM_BACKING_H
