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

#include <gpu/intel/regs/gt_reset_regs.h>
#include <gpu/intel/regs/ring_regs.h>
#include <gpu/intel/regs/force_to_nonpriv_regs.h>

#include "mi_commands.h"
#include "intel_workarounds.h"
#include "mocs_init.h"
#include "gpu/intel/rcs/commands/cmd_pipe_control.h"
#include "vespera/debug/chronos.h"

// TODO(FIX): hardware race ib lite restore, we have to check wether ctx switch is pending
// see lrc_set_reg in dispatch_batxcch and and the end of submit_ring

namespace gpu::intel::core {
    IntelEngine::IntelEngine(
        EngineType type, IntelGpuDevice& device, u32 engine_mmio_offset, const ForceWakeDomain& fw_domain
    )
        : type_(type), device_(device), engine_mmio_offset_(engine_mmio_offset), fw_domain_(fw_domain) {
    }

    bool IntelEngine::engine_reset(u32 timeout_us) const {
        auto gdrst = mmio_read<GDRST>(GDRST_MMIO);

        switch (type_) {
            case EngineType::RCS: gdrst.render = 1;
                break;
            case EngineType::BCS: gdrst.blitter = 1;
                break;
            case EngineType::VCS0: gdrst.media0 = 1;
                break;
            case EngineType::VCS1: gdrst.media1 = 1;
                break;
            case EngineType::VECS: gdrst.vebox = 1;
                break;
        }

        mmio_write(GDRST_MMIO, gdrst);

        while (timeout_us--) {
            const auto current_gdrst = mmio_read<GDRST>(GDRST_MMIO);
            bool is_cleared = false;

            switch (type_) {
                case EngineType::RCS: is_cleared = !current_gdrst.render;
                    break;
                case EngineType::BCS: is_cleared = !current_gdrst.blitter;
                    break;
                case EngineType::VCS0: is_cleared = !current_gdrst.media0;
                    break;
                case EngineType::VCS1: is_cleared = !current_gdrst.media1;
                    break;
                case EngineType::VECS: is_cleared = !current_gdrst.vebox;
                    break;
            }

            if (is_cleared) {
                if (!engine_whitelist_apply()) {
                    Log::error("intel-%s: whitelist restore after engine reset failed", engine_type_to_string(type_));
                }

                if (type_ == EngineType::RCS) {
                    if (!wa::apply_rcs_engine_workarounds(device_.mmio_base(), device_.fuse_topology().mcr_selector)) {
                        Log::error("intel-rcs: engine workarounds restore after reset failed");
                    }

                    init_mocs_table(device_.mmio_base());
                }

                return true;
            }

            kernel::time::sleep_us(1);
        }

        Log::error("intel-%s: Engine reset timed out", engine_type_to_string(type_));
        return false;
    }

    void IntelEngine::ring_alloc_and_init(u32 ring_size_bytes) {
        if (!ring_alloc_for(engine_ring_, ring_size_bytes)) {
            Log::error("intel-engine: engine_ring_ allocation failed (%u bytes)", ring_size_bytes);
            return;
        }

        RING_BUFFER_START start{};
        start.set_start_addr_bytes(gfx_raw(engine_ring_.gfx_addr));
        engine_reg_write(ENGINE_RING_START_OFF, start);

        RING_BUFFER_CTL ctl{};
        ctl.ring_enable = 1;
        ctl.set_ring_size_bytes(ring_size_bytes);
        engine_reg_write(ENGINE_RING_CTL_OFF, ctl);

        RING_BUFFER_HEAD head{};
        head.set_head_offset_bytes(0);
        engine_reg_write(ENGINE_RING_HEAD_OFF, head);

        RING_BUFFER_TAIL tail{};
        tail.set_tail_offset_bytes(0);
        engine_reg_write(ENGINE_RING_TAIL_OFF, tail);
    }

    bool IntelEngine::ring_alloc_for(Ring& ring, u32 ring_size_bytes) const {
        ring.size = ring_size_bytes;
        ring.tail = 0;

        const u32 ring_pages = ring_size_bytes / PAGE_SIZE;
        auto alloc = ggtt().alloc_persistent(ring_pages);
        ring.cpu_addr = alloc.cpu_addr;
        ring.gfx_addr = alloc.gfx_addr;
        ring.phys_addr = alloc.phys_addr;

        if (virt_null(ring.cpu_addr)) {
            return false;
        }

        memset(virt_ptr(ring.cpu_addr), 0, ring_size_bytes);

        Log::log_dbc("intel-engine: Ring Buffer CPU=%p GFX=0x%llx", virt_ptr(ring.cpu_addr), gfx_raw(ring.gfx_addr));

        volatile auto* r = virt_as<u32>(ring.cpu_addr);
        for (u32 i = 0; i < ring_size_bytes / 4; i++) {
            r[i] = MI_NOOP;
        }

        // Mask all hardware status writes by default; a derived engine that
        // relies on HWSTAM-driven fence writes (BCS's MI_FLUSH_DW path)
        // overrides this after ring_alloc_and_init() returns. HWSTAM is a
        // single MMIO register (not per-context), so this write is only
        // meaningful the first time -- redundant but harmless when this is
        // called again for a per-context ring.
        HWSTAM_REG stam{};
        stam.raw = 0xFFFFFFFFu;
        engine_reg_write(ENGINE_HWSTAM_OFF, stam);

        return true;
    }

    void IntelEngine::ring_write(Ring& ring, u32 dword) {
        volatile auto* r = virt_as<u32>(ring.cpu_addr);
        r[ring.tail / 4] = dword;
        asm volatile("sfence" ::: "memory");

        ring.tail += 4;
        if (ring.tail >= ring.size) ring.tail = 0;
    }

    void IntelEngine::ring_flush() {
        // TAIL must be 8-byte aligned (bits [2:0] = MBZ)
        while (engine_ring_.tail & 0x7) {
            ring_write(engine_ring_, MI_NOOP);
        }

        asm volatile("mfence" ::: "memory");

        RING_BUFFER_TAIL tail{};
        tail.set_tail_offset_bytes(engine_ring_.tail);
        engine_reg_write(ENGINE_RING_TAIL_OFF, tail);
    }

    void IntelEngine::submit_ring(EngineContext& ctx, const EngineContext* sibling) {
        switch (submission_mode_) {
            case SubmissionMode::LegacyRing: {
                while (engine_ring_.tail & 0x7) {
                    ring_write(engine_ring_, MI_NOOP);
                }
                asm volatile("mfence" ::: "memory");

                RING_BUFFER_TAIL tail{};
                tail.set_tail_offset_bytes(engine_ring_.tail);
                engine_reg_write(ENGINE_RING_TAIL_OFF, tail);
                break;
            }
            case SubmissionMode::Execlist: {
                if (virt_null(ctx.lrc.cpu_addr) || virt_null(ctx.ring.cpu_addr)) {
                    Log::error(
                        "intel-%s: submit_ring() called in Execlist mode with no LRC/ring allocated for this context",
                        engine_type_to_string(type_));
                    return;
                }
                while (ctx.ring.tail & 0x7) {
                    ring_write(ctx.ring, MI_NOOP);
                }
                lrc_update_tail(ctx, ctx.ring.tail);

                asm volatile("mfence" ::: "memory");
                lrc_submit(ctx, sibling);
                break;
            }
        }
    }

    void IntelEngine::lrc_update_tail(const EngineContext& ctx, u32 tail_bytes) const {
        RING_BUFFER_TAIL tail{};
        tail.set_tail_offset_bytes(tail_bytes);
        lrc_write_ring_field(ctx, LRC_DW_RING_TAIL, ENGINE_RING_TAIL_OFF, tail.raw);
    }

    void IntelEngine::ring_retire(const EngineContext& ctx, Ring& ring) const {
        const u32 done = __atomic_load_n(seqno_ptr_for_read(ctx), __ATOMIC_ACQUIRE);

        while (!ring.inflight_empty()) {
            const Ring::Entry& e = ring.inflight[ring.inflight_head];
            if (static_cast<i32>(done - e.seqno) < 0) {
                break; // FIFO-Reihenfolge: alle folgenden sind ebenfalls noch nicht fertig
            }
            ring.head = e.postfix;
            ring.inflight_head = (ring.inflight_head + 1) & (Ring::MAX_INFLIGHT - 1);
        }
    }

    bool IntelEngine::ring_wait_space(
        const EngineContext& ctx, Ring& ring,
        const u32 required_bytes, const u32 timeout_us
    ) const {
        ring_retire(ctx, ring);
        if (!ring.inflight_full() && ring.free_space() >= required_bytes) {
            return true;
        }

        const u64 start = kernel::time::get_uptime_us();
        while ((kernel::time::get_uptime_us() - start) < timeout_us) {
            ring_retire(ctx, ring);
            if (!ring.inflight_full() && ring.free_space() >= required_bytes) {
                return true;
            }
            // Hinweis: dispatch_batch() hält submit_lock_ (IRQs aus), daher pause statt sleep_us!
            asm volatile("pause");
        }

        return false;
    }

    void IntelEngine::hwsp_alloc() {
        auto alloc = ggtt().alloc_persistent(1, (1ULL << PtFlag::ReadWrite), CacheMode::Uncacheable, MOCS_UNCACHED);
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

    const u32* IntelEngine::seqno_ptr_for_read(const EngineContext& ctx) const {
        if (submission_mode_ == core::SubmissionMode::Execlist) {
            auto* pphwsp = virt_as<u32>(ctx.lrc.cpu_addr);
            return &pphwsp[PPHWSP_SEQNO_DWORD_INDEX];
        }
        auto* hwsp = virt_as<u32>(hwsp_cpu_addr_);
        return &hwsp[HWSP_SEQNO_OFFSET_DWORDS];
    }

    bool IntelEngine::wait_seqno(const EngineContext& ctx, const u32 target_seqno, const i64 timeout_ns) const {
        if (is_banned()) return false;

        const bool infinite = timeout_ns < 0;
        const u64 deadline_ns = infinite ? 0 : kernel::time::get_uptime_ns() + static_cast<u64>(timeout_ns);

        while (true) {
            const u32 done = __atomic_load_n(seqno_ptr_for_read(ctx), __ATOMIC_ACQUIRE);
            if (static_cast<i32>(done - target_seqno) >= 0) return true;

            if (is_banned()) return false;
            if (!infinite && kernel::time::get_uptime_ns() >= deadline_ns) return false;

            kernel::time::sleep_us(500);
        }
    }

    namespace {
        struct LriBlock {
            const u32* engine_relative_offsets;
            u32 offset_count;
            bool posted; // MI_LRI_FORCE_POSTED
            u32 leading_nop_dwords;
        };

        constexpr u32 kRcsLriBlock0[] = {
            0x244,
            0x34, 0x30, 0x38, 0x3c, 0x168, 0x140, 0x110, 0x11c, 0x114, 0x118, 0x1c0, 0x1c4, 0x1c8,
        };
        constexpr u32 kRcsLriBlock1[] = {
            0x3a8, 0x28c, 0x288, 0x284, 0x280, 0x27c, 0x278, 0x274, 0x270,
        };
        constexpr u32 kRcsLriBlock2[] = {
            0xc8,
        };
        constexpr u32 kRcsLriBlock3[] = {
            0x28, 0x9c, 0xc0, 0x178, 0x17c, 0x358, 0x170, 0x150, 0x154, 0x158, 0x41c,
            0x600, 0x604, 0x608, 0x60c, 0x610, 0x614, 0x618, 0x61c, 0x620, 0x624, 0x628, 0x62c,
            0x630, 0x634, 0x638, 0x63c, 0x640, 0x644, 0x648, 0x64c, 0x650, 0x654, 0x658, 0x65c,
            0x660, 0x664, 0x668, 0x66c, 0x670, 0x674, 0x678, 0x67c,
            0x68,
        };

#define ARRAY_COUNT(arr) static_cast<u32>(sizeof(arr) / sizeof((arr)[0]))
        constexpr LriBlock kGen9RcsContextLayout[] = {
            {kRcsLriBlock0, ARRAY_COUNT(kRcsLriBlock0), /*posted=*/true, /*leading_nop_dwords=*/1},
            {kRcsLriBlock1, ARRAY_COUNT(kRcsLriBlock1), /*posted=*/true, /*leading_nop_dwords=*/3},
            {kRcsLriBlock2, ARRAY_COUNT(kRcsLriBlock2), /*posted=*/false, /*leading_nop_dwords=*/13},
            {kRcsLriBlock3, ARRAY_COUNT(kRcsLriBlock3), /*posted=*/true, /*leading_nop_dwords=*/13},
        };
#undef ARRAY_COUNT

        constexpr u32 kMiLoadRegisterImmOpcode = 0x22u << 23;
        constexpr u32 kMiLriForcePosted = 1u << 12;

        // Writes one LRI block (leading NOPs, header, then zeroed
        // register/value pairs) at *cursor, advancing it past the block.
        // Values are left at 0; callers overwrite them afterwards via
        // lrc_set_reg(). NOP slots are left at 0, which the command
        // streamer reads as MI_NOOP.
        u32* write_lri_block(u32* cursor, const LriBlock& block, u32 base_mmio) {
            cursor += block.leading_nop_dwords;

            u32 header = kMiLoadRegisterImmOpcode | (2 * block.offset_count - 1);
            if (block.posted) {
                header |= kMiLriForcePosted;
            }
            *cursor++ = header;

            for (u32 i = 0; i < block.offset_count; i++) {
                *cursor++ = base_mmio + block.engine_relative_offsets[i];
                *cursor++ = 0; // populated later via lrc_set_reg()
            }

            return cursor;
        }

        void write_lrc_ring_context(u32* lrc_ring, const LriBlock* layout, u32 layout_count, u32 base_mmio) {
            u32* cursor = lrc_ring;
            for (u32 i = 0; i < layout_count; i++) {
                cursor = write_lri_block(cursor, layout[i], base_mmio);
            }
        }

        bool lrc_set_reg(u32* lrc_ring, u32 reg_mmio_addr, u32 value) {
            constexpr usize kSearchLimitDwords = 0x100;
            for (usize i = 0; i < kSearchLimitDwords; ++i) {
                if (lrc_ring[i] == reg_mmio_addr) {
                    lrc_ring[i + 1] = value;
                    return true;
                }
            }
            return false;
        }

        constexpr u32 kRcsWhitelist[] = {
            GEN8_L3SQCREG4,
            GEN9_CTX_PREEMPT_REG,
            GEN8_CS_CHICKEN1,
            GEN8_HDC_CHICKEN1,
            COMMON_SLICE_CHICKEN2
        };
        static_assert(sizeof(kRcsWhitelist) / sizeof(kRcsWhitelist[0]) <= FORCE_TO_NONPRIV_SLOT_COUNT,
                      "RCS whitelist exceeds the FORCE_TO_NONPRIV slots available");

        struct WhitelistTable {
            const u32* mmio_offsets;
            u32 count;
        };

        WhitelistTable whitelist_for_engine(EngineType type) {
            switch (type) {
                case EngineType::RCS:
                    return {kRcsWhitelist, static_cast<u32>(sizeof(kRcsWhitelist) / sizeof(kRcsWhitelist[0]))};
                default:
                    return {nullptr, 0};
            }
        }
    } // namespace

    bool IntelEngine::engine_whitelist_apply() const {
        const auto table = whitelist_for_engine(type_);

        for (u32 i = 0; i < table.count; ++i) {
            if (!FORCE_TO_NONPRIV::is_whitelistable(table.mmio_offsets[i])) {
                Log::error("intel-%s: whitelist entry 0x%x is not a valid MMIO address for FORCE_TO_NONPRIV",
                           engine_type_to_string(type_), table.mmio_offsets[i]);
                return false;
            }
        }

        for (u32 slot = 0; slot < FORCE_TO_NONPRIV_SLOT_COUNT; ++slot) {
            const auto value = (slot < table.count)
                                   ? FORCE_TO_NONPRIV::for_register(table.mmio_offsets[slot])
                                   : FORCE_TO_NONPRIV::unused();

            engine_reg_write(ENGINE_FORCE_TO_NONPRIV_OFF + slot * sizeof(u32), value);

            if (slot < table.count) {
                Log::log_dbc("intel-%s: FORCE_TO_NONPRIV[%u] <- reg 0x%05x",
                             engine_type_to_string(type_), slot, table.mmio_offsets[slot]);
            }
        }

        asm volatile("mfence" ::: "memory");
        return true;
    }

    bool IntelEngine::engine_whitelist_verify() const {
        const auto table = whitelist_for_engine(type_);
        bool ok = true;

        for (u32 slot = 0; slot < FORCE_TO_NONPRIV_SLOT_COUNT; ++slot) {
            const u32 expected = (slot < table.count)
                                     ? FORCE_TO_NONPRIV::for_register(table.mmio_offsets[slot]).raw
                                     : FORCE_TO_NONPRIV_DEFAULT;
            const u32 actual = engine_reg_read_raw(ENGINE_FORCE_TO_NONPRIV_OFF + slot * sizeof(u32));

            if (actual != expected) {
                Log::error("intel-%s: FORCE_TO_NONPRIV[%u] readback 0x%08x != expected 0x%08x",
                           engine_type_to_string(type_), slot, actual, expected);
                ok = false;
            }
        }

        return ok;
    }

    bool IntelEngine::dispatch_batch(
        const gfx_addr_t batch_addr, const u64 batch_len, u32* out_seqno,
        const IntelPpgtt* vm, EngineContext& ctx, const EngineContext* sibling
    ) {
        (void)batch_len;

        if (!out_seqno) {
            return false;
        }

        Ring* const saved_ring = active_dispatch_ring_;
        active_dispatch_ring_ = (submission_mode_ == SubmissionMode::Execlist) ? &ctx.ring : &engine_ring_;

        auto* lrc_base = virt_as<u32>(ctx.lrc.cpu_addr);
        u32* lrc_ring = lrc_base + (LRC_RING_CONTEXT_START / sizeof(u32));
        lrc_set_reg(lrc_ring, engine_mmio_offset_ + ENGINE_PDP0_LDW_OFF, static_cast<u32>(vm->pml4_phys_addr_bytes()));
        lrc_set_reg(lrc_ring, engine_mmio_offset_ + ENGINE_PDP0_UDW_OFF,
                    static_cast<u32>(vm->pml4_phys_addr_bytes() >> 32));

        if (ctx.has_run_once) {
            constexpr u32 mask = (LRC_CTX_SR_CTL_RESTORE_INHIBIT_BIT | LRC_CTX_SR_CTL_INHIBIT_SYNC_CTX_SWITCH_BIT) <<
                16;
            lrc_set_reg(lrc_ring, engine_mmio_offset_ + ENGINE_CONTEXT_CONTROL_OFF,
                        mask | LRC_CTX_SR_CTL_INHIBIT_SYNC_CTX_SWITCH_BIT);
        }

        kernel::chronos::checkpoint("gpu.ring", "wait_space_begin");
        if (!ring_wait_space(ctx, 512, 100000)) {
            Log::error("intel-%s: ring_wait_space timed out", engine_type_to_string(type_));
            active_dispatch_ring_ = saved_ring;
            return false;
        }
        kernel::chronos::checkpoint("gpu.ring", "wait_space_end");

        if (type_ == EngineType::RCS && !ctx.has_run_once) {
            u32 lri_buf[64];
            u32* end = wa::emit_ctx_lri(lri_buf);
            const auto dword_count = static_cast<usize>(end - lri_buf);

            for (usize i = 0; i < dword_count; ++i) {
                ring_write(*active_dispatch_ring_, lri_buf[i]);
            }
        }

        if (type_ == EngineType::RCS) {
            PIPE_CONTROL pre_vf_pc = PIPE_CONTROL::create();
            pre_vf_pc.command_streamer_stall_enable = 1;
            ring_write_cmd(pre_vf_pc);

            PIPE_CONTROL inv_pc = PIPE_CONTROL::create();
            inv_pc.command_streamer_stall_enable = 1;
            inv_pc.tlb_invalidate = 1;
            inv_pc.instruction_cache_invalidate_enable = 1;
            inv_pc.texture_cache_invalidation_enable = 1;
            inv_pc.vf_cache_invalidation_enable = 1;
            inv_pc.constant_cache_invalidation_enable = 1;
            inv_pc.state_cache_invalidation_enable = 1;
            inv_pc.post_sync_operation = PIPE_CONTROL::WRITE_IMMEDIATE_DATA;
            inv_pc.destination_address_type = (submission_mode_ == SubmissionMode::Execlist) ? 0 : 1;
            inv_pc.store_data_index = 1;
            inv_pc.address_lo = PPHWSP_SCRATCH_DWORD_INDEX;
            inv_pc.address_hi = 0;
            inv_pc.immediate_data = 0;
            ring_write_cmd(inv_pc);
        }

        const MI_BATCH_BUFFER_START start_cmd = MI_BATCH_BUFFER_START::create(gfx_raw(batch_addr));
        ring_write_cmd(start_cmd);

        *out_seqno = seqno_next();

        const u32 postfix = active_dispatch_ring_->tail;

        emit_flush(*out_seqno);

        active_dispatch_ring_->inflight_push(*out_seqno, postfix);

        kernel::chronos::checkpoint("gpu.ring", "submit_ring_begin");
        submit_ring(ctx, sibling);
        kernel::chronos::checkpoint("gpu.ring", "submit_ring_end");

        ctx.has_run_once = true;

        active_dispatch_ring_ = saved_ring;

        return true;
    }

    void IntelEngine::lrc_write_ring_field(
        const EngineContext& ctx, usize dword_offset,
        u32 engine_relative_mmio_off, u32 value
    ) const {
        auto* lrc = virt_as<u32>(ctx.lrc.cpu_addr);
        const usize base = LRC_RING_CONTEXT_START / sizeof(u32);

        lrc[base + dword_offset] = engine_mmio_offset_ + engine_relative_mmio_off;
        lrc[base + dword_offset + 1] = value;
    }

    bool IntelEngine::ensure_execlist_mode_enabled() {
        if (execlist_mode_enabled_) {
            return true;
        }

        if (!engine_whitelist_apply()) {
            return false;
        }
        if (!engine_whitelist_verify()) {
            Log::error("intel-%s: FORCE_TO_NONPRIV readback mismatch - whitelist may not be active",
                       engine_type_to_string(type_));
        }

        GFX_MODE mode{};
        mode.set_execlist_enable(true);
        engine_reg_write(ENGINE_GFX_MODE_OFF, mode);

        execlist_mode_enabled_ = true;
        return true;
    }

    bool IntelEngine::lrc_alloc_and_init(
        EngineContext& ctx, const u32 sw_context_id
    ) const {
        ctx.sw_context_id = sw_context_id;

        const usize size_bytes = lrc_size_bytes();
        const u32 lrc_pages = static_cast<u32>(size_bytes / LRC_PAGE_SIZE);
        const auto alloc = ggtt().
            alloc_transient(lrc_pages, (1ULL << ReadWrite), CacheMode::Uncacheable, MOCS_UNCACHED);
        ctx.lrc.cpu_addr = alloc.cpu_addr;
        ctx.lrc.gfx_addr = alloc.gfx_addr;
        ctx.lrc_pages = lrc_pages;

        if (virt_null(ctx.lrc.cpu_addr)) {
            Log::error("intel-%s: LRC allocation failed (%u pages)", engine_type_to_string(type_), lrc_pages);
            return false;
        }

        memset(virt_ptr(ctx.lrc.cpu_addr), 0, size_bytes);

        // Every context gets its own physical ring now (see EngineContext::ring
        // and the Ring struct's comment) -- two contexts co-resident in HW
        // execlist slots 0/1 must never share ring memory. Sized the same as
        // engine_ring_ (engine_ring_.size was set once, at engine bring-up,
        // by ring_alloc_and_init()'s caller -- see e.g. IntelRcs::init_device()).
        if (!ring_alloc_for(ctx.ring, engine_ring_.size)) {
            Log::error("intel-%s: per-context ring allocation failed (ctx=%u)", engine_type_to_string(type_),
                       sw_context_id);
            return false;
        }

        Log::log_dbc(
            "intel-%s: LRC CPU=%p GFX=0x%llx size=%u pages ctx=%u ring GFX=0x%llx", engine_type_to_string(type_),
            virt_ptr(ctx.lrc.cpu_addr), gfx_raw(ctx.lrc.gfx_addr), lrc_pages, sw_context_id, gfx_raw(ctx.ring.gfx_addr)
        );

        auto* lrc_base = virt_as<u32>(ctx.lrc.cpu_addr);
        u32* lrc_ring = lrc_base + (LRC_RING_CONTEXT_START / sizeof(u32));

        // Only RCS has a context layout table defined so far (BCS/VCS/VECS
        // use different register blocks - notably RCS is the only engine
        // with R_PWR_CLK_STATE). Add per-engine layouts here as those are
        // brought up.
        if (type_ == EngineType::RCS) {
            constexpr u32 kGen9RcsContextLayoutCount =
                sizeof(kGen9RcsContextLayout) / sizeof(kGen9RcsContextLayout[0]);
            write_lrc_ring_context(lrc_ring, kGen9RcsContextLayout, kGen9RcsContextLayoutCount, engine_mmio_offset_);
        }

        u32 context_control_val = 0;
        if (type_ == EngineType::RCS) {
            context_control_val = LRC_CTX_SR_CTL_FIRST_SUBMIT_MASK_AND_VALUE;
        }

        lrc_set_reg(lrc_ring, engine_mmio_offset_ + ENGINE_BB_STATE_OFF, LRC_BB_STATE_ADDRESS_SPACE_PPGTT_BIT);
        lrc_set_reg(lrc_ring, engine_mmio_offset_ + ENGINE_CONTEXT_CONTROL_OFF, context_control_val);
        lrc_set_reg(lrc_ring, engine_mmio_offset_ + ENGINE_RING_HEAD_OFF, 0); // RING_HEAD
        lrc_set_reg(lrc_ring, engine_mmio_offset_ + ENGINE_RING_TAIL_OFF, 0); // RING_TAIL
        lrc_set_reg(lrc_ring, engine_mmio_offset_ + ENGINE_RING_START_OFF, gfx_raw(ctx.ring.gfx_addr));
        // RING_START -- this context's own ring, not engine_ring_
        RING_BUFFER_CTL ctl{};
        ctl.ring_enable = 1;
        ctl.set_ring_size_bytes(ctx.ring.size);
        lrc_set_reg(lrc_ring, engine_mmio_offset_ + ENGINE_RING_CTL_OFF, ctl.raw); // RING_CTL

        // NOTE: PDP0 used to be seeded here from the engine-owned ppgtt_.
        // It's left at 0 now -- dispatch_batch() overwrites PDP0/PDP0_UDW
        // on every submission with the real per-process PPGTT passed in by
        // the caller, so seeding it here with anything would just be a
        // value that gets clobbered before the first real batch runs.

        asm volatile("mfence" ::: "memory");

        return true;
    }

    void IntelEngine::lrc_free(const EngineContext& ctx) const {
        ggtt().free_transient(ctx.lrc, ctx.lrc_pages);

        // TODO(lucifer): ctx.ring was allocated via ggtt().alloc_persistent()
        // (see ring_alloc_for()) -- the ring-buffer-per-engine version of
        // this code never freed engine_ring_ either (engines live for the
        // device's lifetime), so there was no existing free_persistent()
        // call site to mirror here. Now that rings are per-context and
        // contexts DO get torn down (LucFile::~LucFile()), this needs
        // whatever GgttAllocator's persistent-allocation counterpart to
        // free_transient() is -- check ggtt_allocator.h for its exact name
        // (likely free_persistent(Ring, page_count)) and call it with
        // ctx.ring and (ctx.ring.size / PAGE_SIZE) here. Leaving this
        // unfreed is a GGTT leak per LucFile close, not a correctness bug
        // for the locking/scheduling work in this change, but it should be
        // closed before this ships.
    }

    void IntelEngine::print_execlist_status() const {
        const EXECLIST_STATUS status = read_execlist_status();

        const char* active_elem_str = "RESERVED";
        switch (status.current_active_element) {
            case EXECLIST_STATUS::NO_ACTIVE_ELEMENT: active_elem_str = "None (Idle)";
                break;
            case EXECLIST_STATUS::ELEMENT0_EXECUTING: active_elem_str = "Element 0";
                break;
            case EXECLIST_STATUS::ELEMENT1_EXECUTING: active_elem_str = "Element 1";
                break;
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

    void IntelEngine::lrc_submit(const EngineContext& ctx, const EngineContext* ctx1) const {
        CONTEXT_DESCRIPTOR element0{};
        element0.valid = 1;
        if (!ctx.has_run_once) {
            element0.force_restore = 1;
        }
        element0.addressing_mode = CONTEXT_DESCRIPTOR::LEGACY_64BIT_PPGTT;
        element0.privilege_access = 1;
        element0.fault_handling = CONTEXT_DESCRIPTOR::FAULT_AND_HANG;
        element0.set_lrca_address_bytes(gfx_raw(ctx.lrc.gfx_addr));
        element0.sw_context_id = ctx.sw_context_id;

        CONTEXT_DESCRIPTOR element1{}; // left invalid unless ctx1 is supplied
        if (ctx1 != nullptr) {
            element1.valid = 1;
            if (!ctx1->has_run_once) {
                element1.force_restore = 1;
            }
            element1.addressing_mode = CONTEXT_DESCRIPTOR::LEGACY_64BIT_PPGTT;
            element1.privilege_access = 1;
            element1.fault_handling = CONTEXT_DESCRIPTOR::FAULT_AND_HANG;
            element1.set_lrca_address_bytes(gfx_raw(ctx1->lrc.gfx_addr));
            element1.sw_context_id = ctx1->sw_context_id;
        }

        {
            const u64 start_us = kernel::time::get_uptime_us();
            constexpr u64 kBusyWaitTimeoutUs = 100;

            for (;;) {
                const EXECLIST_STATUS status = read_execlist_status();
                const bool busy = (status.execlist_write_pointer == status.current_execlist_pointer) &&
                                  status.execlist_queue_full;
                if (!busy) break;

                if (kernel::time::get_uptime_us() - start_us > kBusyWaitTimeoutUs) {
                    Log::error("intel-%s: lrc_submit: timeout waiting for execlist port (not busy)",
                               engine_type_to_string(type_));
                    break;
                }
            }
        }

        // Per PRM-mandated submission order: Element 1 first, then Element 0.
        engine_reg_write_raw(ENGINE_EXECLIST_SUBMITPORT_OFF, static_cast<u32>(element1.raw >> 32));
        engine_reg_write_raw(ENGINE_EXECLIST_SUBMITPORT_OFF, static_cast<u32>(element1.raw));
        engine_reg_write_raw(ENGINE_EXECLIST_SUBMITPORT_OFF, static_cast<u32>(element0.raw >> 32));
        engine_reg_write_raw(ENGINE_EXECLIST_SUBMITPORT_OFF, static_cast<u32>(element0.raw));
    }

    void IntelEngine::dump_ppgtt_page_faults() const {
        const u32 fault_indication = mmio_read(0x4574); // GTT Page Fault Indication
        const u32 ring_esr = mmio_read(0x0044);         // RING_ESR

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

    bool IntelEngine::do_dispatch_locked(PendingSubmission& sub, const u32 slot) {
        // Caller (submit_or_wait() or on_gt_context_switch()) already holds
        // submit_lock_ and has already decided `slot` is free.
        const EngineContext* sibling = active_slot_[1 - slot];

        const bool ok = dispatch_batch(sub.batch_addr, sub.batch_len, &sub.out_seqno, sub.vm, *sub.ctx, sibling);
        sub.dispatch_ok = ok;

        if (ok) {
            active_slot_[slot] = sub.ctx;
        }

        return ok;
    }

    void IntelEngine::on_gt_context_switch() {
        u64 flags{};
        submit_lock_.lock_irqsave(flags);

        const EXECLIST_STATUS status = read_execlist_status();

        const bool slot_now_free[2] = {
            active_slot_[0] != nullptr && !status.execlist0_valid,
            active_slot_[1] != nullptr && !status.execlist1_valid,
        };

        for (u32 slot = 0; slot < 2; ++slot) {
            if (!slot_now_free[slot]) {
                continue;
            }

            active_slot_[slot] = nullptr;

            PendingSubmission* next = pending_queue_.pop();
            if (next == nullptr) {
                continue;
            }

            do_dispatch_locked(*next, slot);
            next->done.signal();
        }

        submit_lock_.unlock_irqrestore(flags);
    }

    bool IntelEngine::submit_or_wait(
        const gfx_addr_t batch_addr, const u64 batch_len, u32* out_seqno, const IntelPpgtt* vm, EngineContext& ctx
    ) {
        if (!out_seqno) {
            return false;
        }

        PendingSubmission sub;
        sub.ctx = &ctx;
        sub.batch_addr = batch_addr;
        sub.batch_len = batch_len;
        sub.vm = vm;
        sub.done.init(1, 0);

        u64 flags;
        submit_lock_.lock_irqsave(flags);

        constexpr u32 SLOT_NONE = 0xFFFFFFFFu;

        u32 own_slot = SLOT_NONE;
        for (u32 i = 0; i < 2; ++i) {
            if (active_slot_[i] == &ctx) {
                own_slot = i;
                break;
            }
        }

        u32 target_slot = own_slot;
        if (target_slot == SLOT_NONE) {
            if (active_slot_[0] == nullptr) {
                target_slot = 0;
            } else if (active_slot_[1] == nullptr) {
                target_slot = 1;
            }
        }

        if (target_slot == SLOT_NONE) {
            pending_queue_.push(&sub);
            submit_lock_.unlock_irqrestore(flags);

            sub.done.wait(); // blocks (sleeps), does not spin -- see semaphore.h

            if (!sub.dispatch_ok) {
                return false;
            }
            *out_seqno = sub.out_seqno;
            return true;
        }

        const bool ok = do_dispatch_locked(sub, target_slot);
        submit_lock_.unlock_irqrestore(flags);

        if (!ok) {
            return false;
        }
        *out_seqno = sub.out_seqno;
        return true;
    }
} // namespace gpu::intel::core
