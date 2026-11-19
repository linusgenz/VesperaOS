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
#include "vespera/time.h"
#include "uapi/vespera/dev/dma-buf.h"
#include "vespera/log.h"

#include "kscan.h"

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

    void LucDmaBuf::add_mapping()    { ref(); }
    void LucDmaBuf::remove_mapping() { unref(); }

    void LucDmaBuf::ref() { __atomic_add_fetch(&refs_, 1, __ATOMIC_RELAXED); }

    void LucDmaBuf::unref() {
        if (__atomic_sub_fetch(&refs_, 1, __ATOMIC_ACQ_REL) == 0) {
            delete this;
        }
    }

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

    int LucDmaBuf::ioctl(const u32 request, void* arg) {
        if (request != DMA_BUF_IOCTL_SYNC) return -ENOTTY;

        Log::log_dbc("[dmabuf-sync] obj=%p phys=0x%llx handle_size=%llu last_write_ctx=%p last_write_seqno=%u engine=%u t=%llu\n",
           (void*)obj_, (unsigned long long)phys_raw(obj_->phys_addr), (unsigned long long)obj_->size,
           (void*)obj_->last_write_ctx, obj_->last_write_seqno, obj_->last_engine_class,
           (unsigned long long)kernel::time::get_uptime_us());

        const auto* sync = static_cast<const dma_buf_sync*>(arg);
        if (!sync) return -EINVAL;
        if (sync->flags & ~DMA_BUF_SYNC_VALID_FLAGS_MASK) return -EINVAL;
        if ((sync->flags & DMA_BUF_SYNC_RW) == 0) return -EINVAL;

        if (sync->flags & DMA_BUF_SYNC_END) return 0;

        if (!obj_) return -EINVAL;

        constexpr i64 TIMEOUT_NS = 10'000'000'000ll;

        if (!device_.wait_seqno(obj_->last_engine_class, obj_->last_write_ctx, obj_->last_write_seqno, TIMEOUT_NS))
            return -ETIME;

        const auto* rt = static_cast<const u8*>(virt_ptr(phys_to_virt(obj_->phys_addr)));
        const auto r = kscan::scan(rt, 1280, 320, 200);
        Log::log_dbc("[kscan-sync] seq=%u stamp=%02x bad_lines=%u\n", obj_->last_write_seqno, r.stamp, r.bad_lines);

        return 0;
    }

    void LucDmaBuf::handle_acquire(void* resource) {
        auto* buf = static_cast<LucDmaBuf*>(static_cast<kernel::vm::VmBackingObject*>(resource));
        buf->ref();
    }

    void LucDmaBuf::handle_destroy(void* resource) {
        auto* buf = static_cast<LucDmaBuf*>(static_cast<kernel::vm::VmBackingObject*>(resource));
        buf->unref();
    }
}
