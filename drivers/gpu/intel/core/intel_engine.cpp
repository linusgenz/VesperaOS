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

#include <gpu/intel/bcs/blt_commands.h>
#include <gpu/intel/regs/gt_reset_regs.h>
#include <gpu/intel/regs/ring_regs.h>

#include "mi_commands.h"

namespace gpu::intel::core {

    IntelEngine::IntelEngine(EngineType type, IntelGpuDevice& device, u32 engine_mmio_offset, ForceWakeDomain fw_domain)
        : type_(type),  device_(device), engine_mmio_offset_(engine_mmio_offset), fw_domain_(fw_domain), ppgtt_(ggtt()) {
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
        ring_phys_addr_ = alloc.phys_addr;

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

    void IntelEngine::submit_ring() {
        // TAIL must be 8-byte aligned (bits [2:0] = MBZ) in both modes
        while (ring_tail_ & 0x7) {
            ring_write(MI_NOOP);
        }

        asm volatile("mfence" ::: "memory");

        switch (submission_mode_) {
            case SubmissionMode::LegacyRing: {
                RING_BUFFER_TAIL tail{};
                tail.set_tail_offset_bytes(ring_tail_);
                engine_reg_write(ENGINE_RING_TAIL_OFF, tail);
                break;
            }

            case SubmissionMode::Execlist: {
                if (virt_null(lrc_cpu_addr_)) {
                    Log::error(
                        "intel-%s: submit_ring() called in Execlist mode with no LRC allocated",
                        engine_type_to_string(type_)
                    );
                    return;
                }

                lrc_update_tail(ring_tail_);
                lrc_submit();
                break;
            }
        }
    }

    void IntelEngine::lrc_update_tail(u32 tail_bytes) const {
        RING_BUFFER_TAIL tail{};
        tail.set_tail_offset_bytes(tail_bytes);
        lrc_write_ring_field(LRC_DW_RING_TAIL, ENGINE_RING_TAIL_OFF, tail.raw);
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
        hwsp_phys_addr_ = alloc.phys_addr;

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

    const u32* IntelEngine::seqno_ptr_for_read() const {
        if (submission_mode_ == core::SubmissionMode::Execlist) {
            auto* pphwsp = virt_as<u32>(lrc_cpu_addr_);
            return &pphwsp[PPHWSP_SEQNO_DWORD_INDEX];
        }
        auto* hwsp = virt_as<u32>(hwsp_cpu_addr_);
        return &hwsp[HWSP_SEQNO_OFFSET_DWORDS];
    }

    bool IntelEngine::seqno_wait(u32 target_seqno, u32 timeout_us, AtomicFlag& completion_flag) {
        const u32* seqno_ptr = seqno_ptr_for_read();
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

    void IntelEngine::lrc_write_ring_field(usize dword_offset, u32 engine_relative_mmio_off, u32 value) const {
        auto* lrc = virt_as<u32>(lrc_cpu_addr_);
        const usize base = LRC_RING_CONTEXT_START / sizeof(u32);

        lrc[base + dword_offset] = engine_mmio_offset_ + engine_relative_mmio_off;
        lrc[base + dword_offset + 1] = value;
    }

#define BIT(nr) (1UL << (nr))
#define NOP(x) static_cast<uint8_t>(BIT(7) | (x))
#define LRI(count, flags) static_cast<uint8_t>(((flags) << 6) | (count))
#define POSTED 1
#define REG(x) static_cast<uint8_t>((x) >> 2)
#define REG16(x) \
static_cast<uint8_t>(((x) >> 9) | BIT(7)), \
static_cast<uint8_t>(((x) >> 2) & 0x7f)
#define END 0

    void setup_lrc_offsets(uint32_t* lrc_ring, const uint8_t* data, uint32_t base_mmio, uint32_t gen = 9) {
        uint32_t* regs = lrc_ring;

        while (*data) {
            if (*data & BIT(7)) { // NOP-Block dekodieren
                uint8_t count = *data++ & ~BIT(7);
                regs += count; // Slots bleiben 0x00000000 (MI_NOOP)
                continue;
            }

            uint8_t count = *data & 0x3F;
            uint8_t flags = *data >> 6;
            data++;

            // MI_LOAD_REGISTER_IMM Header berechnen
            uint32_t cmd = (((0x0) << 29) | (0x22) << 23 | (2 * (count) - 1));
            if (flags & POSTED) {
                cmd |= (1u << 12); // MI_LRI_FORCE_POSTED
            }
            if (gen >= 11) {
                cmd |= (1u << 19); // MI_LRI_LRM_CS_MMIO
            }
            *regs++ = cmd;

            // Register-Offsets dekodieren und eintragen
            for (uint8_t i = 0; i < count; ++i) {
                uint32_t offset = 0;
                uint8_t v;
                do {
                    v = *data++;
                    offset = (offset << 7) | (v & 0x7F);
                } while (v & BIT(7));

                regs[0] = base_mmio + (offset << 2); // MMIO-Zieladresse
                regs[1] = 0;                        // Standardwert (Value)
                regs += 2;
            }
        }
    }

    static const u8 gen9_rcs_offsets[] = {
        NOP(1),
        LRI(14, POSTED),
        REG16(0x244),
        REG(0x34),
        REG(0x30),
        REG(0x38),
        REG(0x3c),
        REG(0x168),
        REG(0x140),
        REG(0x110),
        REG(0x11c),
        REG(0x114),
        REG(0x118),
        REG(0x1c0),
        REG(0x1c4),
        REG(0x1c8),

        NOP(3),
        LRI(9, POSTED),
        REG16(0x3a8),
        REG16(0x28c),
        REG16(0x288),
        REG16(0x284),
        REG16(0x280),
        REG16(0x27c),
        REG16(0x278),
        REG16(0x274),
        REG16(0x270),

        NOP(13),
        LRI(1, 0),
        REG(0xc8),

        NOP(13),
        LRI(44, POSTED),
        REG(0x28),
        REG(0x9c),
        REG(0xc0),
        REG(0x178),
        REG(0x17c),
        REG16(0x358),
        REG(0x170),
        REG(0x150),
        REG(0x154),
        REG(0x158),
        REG16(0x41c),
        REG16(0x600),
        REG16(0x604),
        REG16(0x608),
        REG16(0x60c),
        REG16(0x610),
        REG16(0x614),
        REG16(0x618),
        REG16(0x61c),
        REG16(0x620),
        REG16(0x624),
        REG16(0x628),
        REG16(0x62c),
        REG16(0x630),
        REG16(0x634),
        REG16(0x638),
        REG16(0x63c),
        REG16(0x640),
        REG16(0x644),
        REG16(0x648),
        REG16(0x64c),
        REG16(0x650),
        REG16(0x654),
        REG16(0x658),
        REG16(0x65c),
        REG16(0x660),
        REG16(0x664),
        REG16(0x668),
        REG16(0x66c),
        REG16(0x670),
        REG16(0x674),
        REG16(0x678),
        REG16(0x67c),
        REG(0x68),

        END
    };

    bool lrc_set_reg(uint32_t* lrc_ring, uint32_t reg_mmio_addr, uint32_t value) {
        for (size_t i = 0; i < 0x100; ++i) {
            if (lrc_ring[i] == reg_mmio_addr) {
                lrc_ring[i + 1] = value;
                return true;
            }
        }
        return false;
    }

    bool IntelEngine::lrc_alloc_and_init(const usize lrc_size_bytes, const u32 sw_context_id) {
        lrc_sw_context_id_ = sw_context_id;

        if (!ppgtt_.init()) {
            Log::error("intel-%s: PPGTT init failed", engine_type_to_string(type_));
            return false;
        }

        const u32 lrc_pages = static_cast<u32>(lrc_size_bytes / LRC_PAGE_SIZE);
        const auto alloc = ggtt().alloc_persistent(lrc_pages);
        lrc_cpu_addr_ = alloc.cpu_addr;
        lrc_gfx_addr_ = alloc.gfx_addr;

        if (virt_null(lrc_cpu_addr_)) {
            Log::error("intel-%s: LRC allocation failed (%u pages)", engine_type_to_string(type_), lrc_pages);
            return false;
        }

        memset(virt_ptr(lrc_cpu_addr_), 0, lrc_size_bytes);

        Log::log_dbc(
            "intel-%s: LRC CPU=%p GFX=0x%llx size=%u pages", engine_type_to_string(type_),
            virt_ptr(lrc_cpu_addr_), gfx_raw(lrc_gfx_addr_), lrc_pages
        );

        auto* lrc_base = virt_as<uint32_t>(lrc_cpu_addr_);
        uint32_t* lrc_ring = lrc_base + (LRC_RING_CONTEXT_START / sizeof(uint32_t));
        setup_lrc_offsets(lrc_ring, gen9_rcs_offsets, engine_mmio_offset_, 9);

        // LOAD_REGISTER_IMM headers - written verbatim as the PRM context tables show them,
        // immediately before their respective (offset, value) block.
    /*    auto* lrc = virt_as<u32>(lrc_cpu_addr_);
        const usize base = LRC_RING_CONTEXT_START / sizeof(u32);
       // lrc[base + 1] = LRC_LRI_HEADER_RING_BLOCK;
       // lrc[base + 0x21] = LRC_LRI_HEADER_PDP_BLOCK;

        // Third LRI block is RCS-only (R_PWR_CLK_STATE) - BCS/VCS/VECS don't have this register
        // and Fuchsia's reference only emits this header when id_ == RENDER_COMMAND_STREAMER.
        if (type_ == EngineType::RCS) {
      //      lrc[base + 0x41] = LRC_LRI_HEADER_RENDER_PWR_CLK_BLOCK;
        }*/

        u32 context_control_val = 0;
        if (type_ == EngineType::RCS) {
            constexpr u32 inhibit_sync_context_switch_bit = 1 << 3;
            constexpr u32 render_context_restore_inhibit_bit = 1 << 0;

            constexpr u32 mask = 0xFFFF0000;
            constexpr u32 bits = inhibit_sync_context_switch_bit | render_context_restore_inhibit_bit;
            context_control_val = mask | bits;
        }

        lrc_set_reg(lrc_ring, engine_mmio_offset_ + ENGINE_CONTEXT_CONTROL_OFF, context_control_val);
        lrc_set_reg(lrc_ring, engine_mmio_offset_ + ENGINE_RING_HEAD_OFF, 0); // RING_HEAD
        lrc_set_reg(lrc_ring, engine_mmio_offset_ + ENGINE_RING_TAIL_OFF, 0); // RING_TAIL
        lrc_set_reg(lrc_ring, engine_mmio_offset_ + ENGINE_RING_START_OFF, gfx_raw(ring_gfx_addr_)); // RING_START
        RING_BUFFER_CTL ctl{};
        ctl.ring_enable = 1;
        ctl.set_ring_size_bytes(ring_size_);
        lrc_set_reg(lrc_ring, engine_mmio_offset_ + 0x003C, ctl.raw);
        lrc_set_reg(lrc_ring, engine_mmio_offset_ + ENGINE_RING_CTL_OFF, ctl.raw); // RING_CTL

        const u64 pml4_addr = ppgtt_.pml4_phys_addr_bytes();
        Log::debug("PML 4 ADDR: %llx", pml4_addr);

        lrc_set_reg(lrc_ring, engine_mmio_offset_ + ENGINE_PDP0_LDW_OFF, static_cast<uint32_t>(pml4_addr));
        lrc_set_reg(lrc_ring, engine_mmio_offset_ + ENGINE_PDP0_UDW_OFF, static_cast<uint32_t>(pml4_addr >> 32));

        /*lrc_write_ring_field(LRC_DW_CONTEXT_CONTROL, ENGINE_CONTEXT_CONTROL_OFF, context_control_val);
        lrc_write_ring_field(LRC_DW_RING_HEAD, ENGINE_RING_HEAD_OFF, 0);
        lrc_write_ring_field(LRC_DW_RING_TAIL, ENGINE_RING_TAIL_OFF, 0);
        lrc_write_ring_field(LRC_DW_RING_BUFFER_START, ENGINE_RING_START_OFF, gfx_raw(ring_gfx_addr_));

        RING_BUFFER_CTL ctl{};
        ctl.ring_enable = 1;
        ctl.set_ring_size_bytes(ring_size_);
        lrc_write_ring_field(LRC_DW_RING_BUFFER_CONTROL, ENGINE_RING_CTL_OFF, ctl.raw);

        lrc_write_ring_field(LRC_DW_BB_CURRENT_HEAD_UDW, ENGINE_BB_CURRENT_HEAD_UDW_OFF, 0);
        lrc_write_ring_field(LRC_DW_BB_CURRENT_HEAD, ENGINE_BB_CURRENT_HEAD_OFF, 0);
        lrc_write_ring_field(LRC_DW_BB_STATE, ENGINE_BB_STATE_OFF, LRC_BB_STATE_ADDRESS_SPACE_PPGTT_BIT);
        lrc_write_ring_field(LRC_DW_SECOND_BB_ADDR_UDW, ENGINE_SECOND_BB_ADDR_UDW_OFF, 0);
        lrc_write_ring_field(LRC_DW_SECOND_BB_ADDR, ENGINE_SECOND_BB_ADDR_OFF, 0);
        lrc_write_ring_field(LRC_DW_SECOND_BB_STATE, ENGINE_SECOND_BB_STATE_OFF, 0);
        lrc_write_ring_field(LRC_DW_BB_PER_CTX_PTR, ENGINE_BB_PER_CTX_PTR_OFF, 0);
        lrc_write_ring_field(LRC_DW_INDIRECT_CTX, ENGINE_INDIRECT_CTX_OFF, 0);
        lrc_write_ring_field(LRC_DW_INDIRECT_CTX_OFFSET, ENGINE_INDIRECT_CTX_OFFSET_OFF, 0);

        lrc_write_ring_field(LRC_DW_CTX_TIMESTAMP, ENGINE_CTX_TIMESTAMP_OFF, 0);
        lrc_write_ring_field(LRC_DW_PDP3_UDW, ENGINE_PDP3_UDW_OFF, 0);
        lrc_write_ring_field(LRC_DW_PDP3_LDW, ENGINE_PDP3_LDW_OFF, 0);
        lrc_write_ring_field(LRC_DW_PDP2_UDW, ENGINE_PDP2_UDW_OFF, 0);
        lrc_write_ring_field(LRC_DW_PDP2_LDW, ENGINE_PDP2_LDW_OFF, 0);
        lrc_write_ring_field(LRC_DW_PDP1_UDW, ENGINE_PDP1_UDW_OFF, 0);
        lrc_write_ring_field(LRC_DW_PDP1_LDW, ENGINE_PDP1_LDW_OFF, 0);

        const u64 pml4_addr = ppgtt_.pml4_phys_addr_bytes();
        Log::debug("PML 4 ADDR: %llx", pml4_addr);
        lrc_write_ring_field(LRC_DW_PDP0_UDW, ENGINE_PDP0_UDW_OFF, static_cast<u32>(pml4_addr >> 32));
        lrc_write_ring_field(LRC_DW_PDP0_LDW, ENGINE_PDP0_LDW_OFF, static_cast<u32>(pml4_addr));

        if (type_ == EngineType::RCS) {
            lrc_write_ring_field(LRC_DW_RENDER_PWR_CLK_STATE, ENGINE_RENDER_PWR_CLK_STATE_OFF, 0);
        }
*/
        asm volatile("mfence" ::: "memory");

        GFX_MODE mode{};
        mode.set_execlist_enable(true);
        engine_reg_write(ENGINE_GFX_MODE_OFF, mode);

        kernel::time::sleep_ms(10);

        return true;
    }

    void IntelEngine::print_execlist_status(u64 reg_value) {
    EXECLIST_STATUS status{.raw = reg_value};

    const char* active_elem_str = "RESERVED";
    switch (status.current_active_element) {
        case EXECLIST_STATUS::NO_ACTIVE_ELEMENT:  active_elem_str = "None (Idle)"; break;
        case EXECLIST_STATUS::ELEMENT0_EXECUTING: active_elem_str = "Element 0"; break;
        case EXECLIST_STATUS::ELEMENT1_EXECUTING: active_elem_str = "Element 1"; break;
    }

    Log::print("=== EXECLIST_STATUS Dump [0x%016llX] ===\n", static_cast<unsigned long long>(status.raw));
    Log::print("  Current Context ID : 0x%08X (%u)\n", status.current_context_id, status.current_context_id);
    Log::print("  Active Element     : %s (0b%02b)\n", active_elem_str, status.current_active_element);
    Log::print("  Execlist 0 Valid   : %s\n", status.execlist0_valid ? "YES" : "NO");
    Log::print("  Execlist 1 Valid   : %s\n", status.execlist1_valid ? "YES" : "NO");
    Log::print("  Queue Status       : %s\n", status.execlist_queue_full ? "FULL" : "EMPTY / Normal");
    Log::print("  Write Pointer      : Slot %u\n", status.execlist_write_pointer);
    Log::print("  Current Pointer    : Slot %u\n", status.current_execlist_pointer);
    Log::print("  Arbitration Enable : %s\n", status.arbitration_enable ? "Enabled" : "Disabled");
    Log::print("  Last Switch Reason : 0x%03X\n", status.last_ctx_switch_reason);

    u32 reason = status.last_ctx_switch_reason;
    if (reason != 0) {
        Log::print("    Details [");
        if (reason & (1 << 0)) Log::print(" Context-Complete");
        if (reason & (1 << 1)) Log::print(" Element-Switch");
        if (reason & (1 << 2)) Log::print(" Preempted");
        if (reason & (1 << 3)) Log::print(" Active-to-Idle");
        if (reason & (1 << 4)) Log::print(" Lite-Restore/Int");
        if (reason & (1 << 5)) Log::print(" Wait-Sync");
        if (reason & (1 << 6)) Log::print(" Wait-Semaphore");
        if (reason & (1 << 7)) Log::print(" Wait-Scanline");
        Log::print(" ]\n");
    }
}

    void IntelEngine::lrc_submit() const {
        CONTEXT_DESCRIPTOR element0{};
        element0.valid = 1;
        element0.force_restore = 1;
        element0.addressing_mode = CONTEXT_DESCRIPTOR::LEGACY_64BIT_PPGTT;
        element0.privilege_access = 1;
        element0.fault_handling = CONTEXT_DESCRIPTOR::FAULT_AND_HANG;
        element0.set_lrca_address_bytes(gfx_raw(lrc_gfx_addr_));
        element0.sw_context_id = lrc_sw_context_id_;

        CONTEXT_DESCRIPTOR element1{};  // left invalid - single-context submission

        {
            const u64 start_us = kernel::time::get_uptime_us();
            constexpr u64 kBusyWaitTimeoutUs = 100;

            for (;;) {
                const u64 raw = engine_reg_read_raw(ENGINE_EXECLIST_STATUS_OFF)
                    | (static_cast<u64>(engine_reg_read_raw(ENGINE_EXECLIST_STATUS_OFF + 4)) << 32);

                const u32 current_ptr = static_cast<u32>(raw >> 0) & 0x1;
                const u32 write_ptr = static_cast<u32>(raw >> 1) & 0x1;
                const u32 queue_full = static_cast<u32>(raw >> 2) & 0x1;

                const bool busy = (write_ptr == current_ptr) && queue_full;
                if (!busy) break;

                if (kernel::time::get_uptime_us() - start_us > kBusyWaitTimeoutUs) {
                    Log::error("intel-%s: lrc_submit: timeout waiting for execlist port (not busy)",
                              engine_type_to_string(type_));
                    break;
                }
            }
        }


        log_lrc_context_image();

        engine_reg_write_raw(ENGINE_EXECLIST_SUBMITPORT_OFF, static_cast<u32>(element1.raw >> 32));
        engine_reg_write_raw(ENGINE_EXECLIST_SUBMITPORT_OFF, static_cast<u32>(element1.raw));
        engine_reg_write_raw(ENGINE_EXECLIST_SUBMITPORT_OFF, static_cast<u32>(element0.raw >> 32));
        engine_reg_write_raw(ENGINE_EXECLIST_SUBMITPORT_OFF, static_cast<u32>(element0.raw));
    }

    void IntelEngine::dump_ppgtt_page_faults() const {
        const u32 fault_indication = mmio_read(0x4574); // GTT Page Fault Indication
        const u32 ring_esr         = mmio_read(0x0044); // RING_ESR

        Log::debug("--- PPGTT PAGE FAULT DIAGNOSTICS ---");
        Log::debug("  RING_ESR:         0x%08x", ring_esr);
        Log::debug("  FAULT_INDICATION: 0x%08x", fault_indication);

        constexpr u32 kPpPfdBase = 0x4580;
        constexpr u32 kMaxEntries = 32;

        for (u32 i = 0; i < kMaxEntries; ++i) {
            if (fault_indication & (1U << i)) {
                const u32 reg_val = engine_reg_read_raw(kPpPfdBase + (i * 4));

                const u64 fault_page_addr = static_cast<u64>(reg_val & 0xFFFFF000);

                Log::error("  [PAGE FAULT DETECTED] Slot %2d: Base-Addr ~ 0x%08llx (Raw: 0x%08x)",
                           i, fault_page_addr, reg_val);
            }
        }
    }

    void IntelEngine::log_lrc_context_image() const {
        const u8* lrc_bytes = static_cast<const u8*>(lrc_cpu_addr_.ptr) + 4096;
        const u32* lrc_dwords = reinterpret_cast<const u32*>(lrc_bytes);

        Log::log_dbc("--- LRC CONTEXT IMAGE HEX DUMP (Offset 0x1000 / +4096) ---");

        for (size_t i = 0; i < 300; i += 4) {
            const u32 byte_off = i * 4;
            Log::log_dbc("[%04x]  0x%08x  0x%08x  0x%08x  0x%08x",
                         byte_off,
                         lrc_dwords[i + 0],
                         lrc_dwords[i + 1],
                         lrc_dwords[i + 2],
                         lrc_dwords[i + 3]);
        }
        Log::log_dbc("-------------------------------------------------------");

        Log::log_dbc("--- PPHWSP HEX DUMP ---");

        const u8* hwsp_bytes = static_cast<const u8*>(lrc_cpu_addr_.ptr);
        const u32* hwsp_dwords = reinterpret_cast<const u32*>(hwsp_bytes);

        for (size_t i = 0; i < 60; i += 4) {
            const u32 byte_off = i * 4;
            Log::log_dbc("[%04x]  0x%08x  0x%08x  0x%08x  0x%08x",
                         byte_off,
                         hwsp_dwords[i + 0],
                         hwsp_dwords[i + 1],
                         hwsp_dwords[i + 2],
                         hwsp_dwords[i + 3]);
        }
        Log::log_dbc("-------------------------------------------------------");
    }
}  // namespace blt
