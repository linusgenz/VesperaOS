// intel_dmabuf.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 02.10.26.
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

#ifndef VESPERAOS_INTEL_DMABUF_H
#define VESPERAOS_INTEL_DMABUF_H

#include <vespera/mm/addr.h>
#include <vespera/mm/vm_backing.h>
#include <vespera/types.h>

#include "vespera/mm/dmabuf.h"

namespace gpu::intel::core {
    class IntelGpuDevice;
    struct GemObject;

    class LucDmaBuf final : public kernel::vm::DmaBuf {
    public:
        explicit LucDmaBuf(GemObject* adopted_obj, IntelGpuDevice& device);
        ~LucDmaBuf() override;

        phys_addr_t get_page(usize offset_in_bytes) override;
        [[nodiscard]] usize get_size() const override;
        [[nodiscard]] CacheMode get_cache_mode() const override;

        [[nodiscard]] GemObject* gem_object() const { return obj_; }

    protected:
        int sync(u64 flags) override;

    private:
        GemObject* obj_;
        IntelGpuDevice& device_;
    };
}
#endif