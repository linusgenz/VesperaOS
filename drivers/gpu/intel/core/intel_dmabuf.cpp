// intel_dmabuf.cpp
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

#include "intel_dmabuf.h"
#include "intel_luc_file.h"
#include "uapi/vespera/dev/dma-buf.h"

namespace gpu::intel::core {
    LucDmaBuf::LucDmaBuf(GemObject* adopted_obj, IntelGpuDevice& device)
        : obj_(adopted_obj), device_(device) {}

    LucDmaBuf::~LucDmaBuf() {
        if (obj_) obj_->dec_ref();
    }

    phys_addr_t LucDmaBuf::get_page(const usize offset_in_bytes) {
        if (!obj_ || obj_->is_userptr || obj_->purged || offset_in_bytes >= obj_->size) {
            return phys_addr_t{};
        }
        return phys_add(obj_->phys_addr, offset_in_bytes); // contiguous
    }

    usize LucDmaBuf::get_size() const { return obj_ ? obj_->size : 0; }

    CacheMode LucDmaBuf::get_cache_mode() const {
        if (!obj_) {
            return CacheMode::WriteBack;
        }

        switch (obj_->cpu_caching) {
            case LUCIFER_GEM_CPU_CACHING_WC:
                return CacheMode::WriteCombining;
            case LUCIFER_GEM_CPU_CACHING_WB:
            default:
                return CacheMode::WriteBack;
        }
    }

    int LucDmaBuf::sync(const u64 flags) {
        if (!obj_) return -EINVAL;

        if (flags & DMA_BUF_SYNC_END) return 0;

        constexpr i64 TIMEOUT_NS = 10'000'000'000ll;

        if (!device_.wait_seqno(obj_->last_engine_class, obj_->last_write_ctx, obj_->last_write_seqno, TIMEOUT_NS)) {
            return -ETIME;
        }

        return 0;
    }
}
