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

namespace gpu::intel::core {
    class IntelGpuDevice;
    struct GemObject;

    /// dma-buf: Handle-Resource, die ein GemObject über Prozess-/File-
    /// Grenzen hinweg teilt. refs_ = Handles (Export + Transfers) + VMAs.
    class LucDmaBuf final : public kernel::vm::VmBackingObject {
    public:
        /// Übernimmt die Referenz auf `adopted_obj` (vom Aufrufer per
        /// gem_get_ref() genommen). Startet mit refs_ = 1 (das erste Handle).
        explicit LucDmaBuf(GemObject* adopted_obj, IntelGpuDevice& device);
        ~LucDmaBuf() override;

        LucDmaBuf(const LucDmaBuf&) = delete;
        LucDmaBuf& operator=(const LucDmaBuf&) = delete;

        phys_addr_t get_page(usize offset_in_bytes) override;
        [[nodiscard]] usize get_size() const override;
        void add_mapping() override;
        void remove_mapping() override;

        [[nodiscard]] GemObject* gem_object() const { return obj_; } // für FD_TO_HANDLE

        int ioctl(u32 request, void* arg);

        /// Callbacks für HandleTable: `resource` ist ein VmBackingObject*.
        static void handle_acquire(void* resource);
        static void handle_destroy(void* resource);

    private:
        void ref();
        void unref();
        CacheMode get_cache_mode() const;

        GemObject* obj_;
        u32 refs_ = 1;

        IntelGpuDevice& device_;
    };
}
#endif