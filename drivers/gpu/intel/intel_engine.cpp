// intel_engine.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
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

#include "intel_engine.h"

#include <klib/string.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/time.h>

#include "blt_commands.h"
#include "gt_reset_regs.h"

namespace blt {

    IntelEngine::IntelEngine(EngineType type, IntelGpuDevice& device, u32 engine_mmio_offset, ForceWakeDomain fw_domain)
        :  type_(type), device_(device), engine_mmio_offset_(engine_mmio_offset), fw_domain_(fw_domain) {
    }

    bool IntelEngine::engine_reset(u32 timeout_us) const {
        auto gdrst = mmio_read<GDRST>(GDRST_MMIO);

        switch (type_) {
            case EngineType::RCS:  gdrst.render  = 1; break;
            case EngineType::BCS:  gdrst.blitter = 1; break;
            case EngineType::VCS0: gdrst.media0  = 1; break;
            case EngineType::VCS1: gdrst.media1  = 1; break;
            case EngineType::VECS: gdrst.vebox   = 1; break;
        }

        mmio_write(GDRST_MMIO, gdrst);

        while (timeout_us--) {
            const auto current_gdrst = mmio_read<GDRST>(GDRST_MMIO);
            bool is_cleared = false;

            switch (type_) {
                case EngineType::RCS:  is_cleared = !current_gdrst.render;  break;
                case EngineType::BCS:  is_cleared = !current_gdrst.blitter; break;
                case EngineType::VCS0: is_cleared = !current_gdrst.media0;  break;
                case EngineType::VCS1: is_cleared = !current_gdrst.media1;  break;
                case EngineType::VECS: is_cleared = !current_gdrst.vebox;   break;
            }

            if (is_cleared) {
                return true;
            }

            kernel::time::sleep_us(1);
        }

        Log::error("intel-%s: Engine reset timed out", engine_type_to_string(type_));
        return false;
    }

    void IntelEngine::ring_alloc_and_init(u32 ring_size_bytes) {
        ring_size_ = ring_size_bytes;
        ring_tail_ = 0;

        const u32 ring_pages = ring_size_ / PAGE_SIZE;
        auto alloc = ggtt().alloc_persistent(ring_pages);
        ring_cpu_addr_ = alloc.cpu_addr;
        ring_gfx_addr_ = alloc.gfx_addr;

        memset(virt_ptr(ring_cpu_addr_), 0, ring_size_);

        Log::log_dbc("intel-engine: Ring Buffer CPU=%p GFX=0x%llx", virt_ptr(ring_cpu_addr_), gfx_raw(ring_gfx_addr_));

        volatile auto* ring = virt_as<u32>(ring_cpu_addr_);
        for (u32 i = 0; i < ring_size_ / 4; i++) {
            ring[i] = MI_NOOP;
        }

        // Mask all hardware status writes by default; a derived engine that
        // relies on HWSTAM-driven fence writes (BCS's MI_FLUSH_DW path)
        // overrides this after ring_alloc_and_init() returns.
        HWSTAM_REG stam{};
        stam.raw = 0xFFFFFFFFu;
        engine_reg_write(ENGINE_HWSTAM_OFF, stam);

        RING_BUFFER_START start{};
        start.set_start_addr_bytes(gfx_raw(ring_gfx_addr_));
        engine_reg_write(ENGINE_RING_START_OFF, start);

        RING_BUFFER_CTL ctl{};
        ctl.ring_enable = 1;
        ctl.set_ring_size_bytes(ring_size_);
        engine_reg_write(ENGINE_RING_CTL_OFF, ctl);

        RING_BUFFER_HEAD head{};
        head.set_head_offset_bytes(0);
        engine_reg_write(ENGINE_RING_HEAD_OFF, head);

        RING_BUFFER_TAIL tail{};
        tail.set_tail_offset_bytes(0);
        engine_reg_write(ENGINE_RING_TAIL_OFF, tail);
    }

    void IntelEngine::ring_write(u32 dword) {
        volatile auto* ring = virt_as<u32>(ring_cpu_addr_);
        ring[ring_tail_ / 4] = dword;
        asm volatile("sfence" ::: "memory");

        ring_tail_ += 4;
        if (ring_tail_ >= ring_size_) ring_tail_ = 0;
    }

    void IntelEngine::ring_flush() {
        // TAIL must be 8-byte aligned (bits [2:0] = MBZ)
        while (ring_tail_ & 0x7) {
            ring_write(MI_NOOP);
        }

        asm volatile("mfence" ::: "memory");

        RING_BUFFER_TAIL tail{};
        tail.set_tail_offset_bytes(ring_tail_);
        engine_reg_write(ENGINE_RING_TAIL_OFF, tail);
    }

    bool IntelEngine::ring_wait_space(u32 required_bytes, u32 timeout_us) const {
        const u64 start = kernel::time::get_uptime_us();

        while ((kernel::time::get_uptime_us() - start) < timeout_us) {
            const auto head_reg = engine_reg_read<RING_BUFFER_HEAD>(ENGINE_RING_HEAD_OFF);
            const u32 head = head_reg.head_offset_bytes();

            const u32 avail = (ring_tail_ >= head) ? (ring_size_ - ring_tail_) + head : head - ring_tail_;

            if (avail >= required_bytes) {
                return true;
            }

            kernel::time::sleep_us(1);
        }

        return false;
    }

    void IntelEngine:: hwsp_alloc() {
        auto alloc = ggtt().alloc_persistent(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
        hwsp_cpu_addr_ = alloc.cpu_addr;
        hwsp_gfx_addr_ = alloc.gfx_addr;

        memset(virt_ptr(hwsp_cpu_addr_), 0, PAGE_SIZE);

        HWS_PGA reg{};
        reg.set_address_bytes(gfx_raw(hwsp_gfx_addr_));
        engine_reg_write(ENGINE_HWS_PGA_OFF, reg);
    }

    u32 IntelEngine::seqno_next() {
        ++sequence_number_;
        if (sequence_number_ & SEQNO_BIT5_MASK) {
            // Round up past the bit-5 block: 0x20-0x3F -> 0x40, 0x60-0x7F -> 0x80, etc.
            sequence_number_ = (sequence_number_ & ~u32{0x3F}) + 0x40u;
        }
        return sequence_number_;
    }

    bool IntelEngine::seqno_wait(u32 target_seqno, u32 timeout_us, AtomicFlag& completion_flag) {
        auto* hwsp = virt_as<u32>(hwsp_cpu_addr_);
        const u32* seqno_ptr = &hwsp[HWSP_SEQNO_OFFSET_DWORDS];

        asm volatile("lfence" ::: "memory");
        if (static_cast<i32>(*seqno_ptr - target_seqno) >= 0) return true;

        const u64 deadline_ms = kernel::time::get_uptime_ms() + (timeout_us + 999) / 1000;

        while (true) {
            if (completion_flag.consume()) {
                asm volatile("lfence" ::: "memory");
                if (static_cast<i32>(*seqno_ptr - target_seqno) >= 0) return true;
            }

            if (kernel::time::get_uptime_ms() >= deadline_ms) {
                error_count_++;
                return false;
            }

            asm volatile("pause" ::: "memory");
        }
    }

    bool IntelEngine::seqno_wait_poll(u32 target_seqno, u32 timeout_us) {
        auto* hwsp = virt_as<volatile u32>(hwsp_cpu_addr_);
        const volatile u32* seqno_ptr = &hwsp[HWSP_SEQNO_OFFSET_DWORDS];

        asm volatile("lfence" ::: "memory");
        if (static_cast<i32>(*seqno_ptr - target_seqno) >= 0) {
            return true;
        }

        const u64 deadline_ms = kernel::time::get_uptime_ms() + (timeout_us + 999) / 1000;

        while (true) {
            asm volatile("lfence" ::: "memory");
            if (static_cast<i32>(*seqno_ptr - target_seqno) >= 0) {
                return true;
            }

            // Timeout Check
            if (kernel::time::get_uptime_ms() >= deadline_ms) {
                error_count_++;
                return false;
            }

            asm volatile("pause" ::: "memory");
        }
    }

}  // namespace blt
