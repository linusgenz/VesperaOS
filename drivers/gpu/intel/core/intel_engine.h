// intel_engine.h
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
#ifndef VESPERAOS_INTEL_ENGINE_H
#define VESPERAOS_INTEL_ENGINE_H

#include <vespera/mm/addr.h>
#include <vespera/sync/atomic.h>
#include <vespera/sync/wait_queue.h>
#include <vespera/types.h>

#include "intel_engine_types.h"
#include "intel_gpu_device.h"
#include "intel_ppgtt.h"
#include "../regs/lrc_layout.h"
#include "../regs/execlist_regs.h"

namespace gpu::intel::core {
    constexpr u32 ENGINE_RING_TAIL_OFF = 0x30;
    constexpr u32 ENGINE_RING_HEAD_OFF = 0x34;
    constexpr u32 ENGINE_RING_START_OFF = 0x38;
    constexpr u32 ENGINE_RING_CTL_OFF = 0x3C;
    constexpr u32 ENGINE_HWS_PGA_OFF = 0x80;
    constexpr u32 ENGINE_HWSTAM_OFF = 0x98;

    constexpr u32 ENGINE_GFX_MODE_OFF = 0x29C;
    constexpr u32 GFX_MODE_EXECLIST_ENABLE_BIT = 1u << 15; ///< paired with the same bit in the write mask (upper word)
    constexpr u32 GFX_MODE_EXECLIST_ENABLE_MASK_AND_VALUE =
        (GFX_MODE_EXECLIST_ENABLE_BIT << 16) | GFX_MODE_EXECLIST_ENABLE_BIT;

    constexpr u32 ENGINE_EXECLIST_SUBMITPORT_OFF = 0x230;
    constexpr u32 ENGINE_EXECLIST_STATUS_OFF = 0x234;

    constexpr u32 HWSP_SEQNO_OFFSET_DWORDS = 4;
    constexpr u32 HWSP_SEQNO_OFFSET = HWSP_SEQNO_OFFSET_DWORDS + 16;

    constexpr u32 PPHWSP_SEQNO_DWORD_INDEX = 34;

    constexpr u32 ENGINE_RING_IPEIR_OFF = 0x64;        ///< Instruction Parser Error Identification
    constexpr u32 ENGINE_RING_IPEHR_OFF = 0x68;        ///< Instruction Parser Error Header (the actual bad DWord0)
    constexpr u32 ENGINE_RING_INSTDONE_OFF = 0x6C;     ///< Per-unit "still executing" bits
    constexpr u32 ENGINE_RING_INSTPS_OFF = 0x70;       ///< Sub-op instruction pointer (RCS pipeline state)
    constexpr u32 ENGINE_RING_ACTHD_OFF = 0x74;        ///< Active Head Pointer -- where the CS is *actually* executing
    constexpr u32 ENGINE_RING_ACTHD_UDW_OFF = 0x5c;    ///< Active Head Pointer (udw)
    constexpr u32 ENGINE_RING_DMA_FADD_OFF = 0x78;     ///< Faulting DMA address (low32)
    constexpr u32 ENGINE_RING_DMA_FADD_UDW_OFF = 0x60; ///< Faulting DMA address (udw)
    constexpr u32 ENGINE_RING_CMD_BUF_CCTL_OFF = 0x84;

    constexpr u32 ENGINE_RING_BBADDR_DIFF_OFF = 0x154;
    constexpr u32 ENGINE_RING_BB_START_OFF = 0x150;
    constexpr u32 ENGINE_RING_BB_START_UDW_OFF = 0x170;
    constexpr u32 ENGINE_RING_BBADDR_OFF = 0x140;
    constexpr u32 ENGINE_RING_BBADDR_UDW_OFF = 0x168;

    // CSB lives at the start of the PPHWSP, as an array of 8-byte entries
    // (DWord0 = context ID, DWord1 = status). EXECLIST_STATUS's Write/Current
    // Pointer fields index into this array. 6 entries is the documented Gen9
    // CSB depth.
    constexpr u32 CSB_NUM_ENTRIES = 6;
    constexpr u32 CSB_ENTRY_DWORDS = 2;

    /// How this engine hands its ring contents to hardware.
    /// - LegacyRing: classic RING_BUFFER_TAIL MMIO write (no LRC involved).
    /// - Execlist:   ring contents unchanged, but the *tail value* is mirrored into the LRC's
    ///               Ring Context and submitted via EXECLIST_SUBMITPORT instead of a direct
    ///               RING_BUFFER_TAIL write. Requires lrc_alloc_and_init() to have run first.
    enum class SubmissionMode {
        LegacyRing,
        Execlist,
    };

    struct EngineContext {
        GgttAllocation lrc;
        u32 lrc_pages{0u};
        u32 sw_context_id{0u};
    };

    /**
     * @brief Common ring-buffer/HWSP/seqno machinery shared by every Intel
     *        command-streamer engine.
     *
     * Does NOT own the device — every engine borrows the same
     * IntelGpuDevice for MMIO base and GGTT.
     */
    class IntelEngine {
    public:
        friend class LucFile;

        IntelEngine(EngineType type, IntelGpuDevice& device, u32 engine_mmio_offset, const ForceWakeDomain& fw_domain);
        virtual ~IntelEngine() = default;

        IntelEngine(const IntelEngine&) = delete;
        IntelEngine& operator=(const IntelEngine&) = delete;

        /// Wakes this engine's ForceWake domain (Render for RCS, Blitter for
        /// BCS, ...). Must succeed before touching any of this engine's
        /// MMIO registers.
        [[nodiscard]] bool engine_force_wake_enable() const {
            return device_.force_wake_enable(fw_domain_);
        }

        /// This engine's bit position within the shared GT0 interrupt
        /// register group (GT0_ISR/IMR/IIR/IER, MMIO 0x44300).
        [[nodiscard]] virtual u32 gt_user_irq_bit() const = 0;

        /// Called by IntelGpuDevice's shared IRQ dispatcher when this
        /// engine's bit is set in GT0_IIR.
        virtual void on_gt_user_interrupt() {
        }

        [[nodiscard]] virtual u32 gt_debug_irq_bitmask() const {
            return 0;
        }

        [[nodiscard]] bool engine_whitelist_verify() const;

        [[nodiscard]] bool dispatch_batch(
            gfx_addr_t batch_addr, u64 batch_len, u32* out_seqno, const IntelPpgtt* vm, EngineContext& ctx
        );

        [[nodiscard]] bool seqno_wait_blocking(u32 target_seqno, i64 timeout_ns, WaitQueue& waiters,
                                                const EngineContext& ctx) const;
        [[nodiscard]] bool engine_whitelist_apply() const;

        [[nodiscard]] const u32* seqno_ptr_for_read(const EngineContext& ctx) const;

        void mark_banned() { banned_.set(); }
        [[nodiscard]] bool is_banned() const { return banned_.load(); }

        void print_execlist_status() const;
        void dump_error_state(const char* label, const EngineContext& ctx) const;
        void dump_ring(u32 dwords_before_head, u32 dwords_after_head) const;

    protected:
        EngineType type_;

        [[nodiscard]] IntelGpuDevice& device() const {
            return device_;
        }

        [[nodiscard]] GgttAllocator& ggtt() const {
            return device_.ggtt();
        }

        [[nodiscard]] bool engine_reset(u32 timeout_us = 10000) const;

        [[nodiscard]] volatile u8* engine_regs() const {
            return device_.mmio_base() + engine_mmio_offset_;
        }

        template <class T>
        [[nodiscard]] T engine_reg_read(u32 offset) const {
            T val;
            val.raw = *reinterpret_cast<volatile u32*>(engine_regs() + offset);
            return val;
        }

        [[nodiscard]] u64 engine_reg_read64(const u32 offset) const {
            const u64 low = engine_reg_read_raw(offset);
            const u64 high = engine_reg_read_raw(offset + 4);
            return (high << 32) | low;
        }

        template <class T>
        void engine_reg_write(u32 offset, T val) const {
            *reinterpret_cast<volatile u32*>(engine_regs() + offset) = val.raw;
        }

        [[nodiscard]] u32 engine_reg_read_raw(u32 offset) const {
            return *reinterpret_cast<volatile u32*>(engine_regs() + offset);
        }

        void engine_reg_write_raw(u32 offset, u32 value) const {
            *reinterpret_cast<volatile u32*>(engine_regs() + offset) = value;
        }

        template <typename T>
        [[nodiscard]] T mmio_read(u32 reg) const {
            T val;
            val.raw = *reinterpret_cast<volatile u32*>(device_.mmio_base() + reg);
            return val;
        }

        [[nodiscard]] u32 mmio_read(u32 reg) const {
            return *reinterpret_cast<volatile u32*>(device_.mmio_base() + reg);
        }

        template <typename T>
        void mmio_write(u32 reg, T val) const {
            *reinterpret_cast<volatile u32*>(device_.mmio_base() + reg) = val.raw;
        }

        /// Allocates the ring in GGTT, zero-fills it with MI_NOOP, and
        /// programs RING_BUFFER_START/CTL/HEAD/TAIL + masks HWSTAM.
        void ring_alloc_and_init(u32 ring_size_bytes);

        void ring_write(u32 dword);

        template <typename T>
        void ring_write_cmd(const T& cmd) {
            static_assert(sizeof(T) % sizeof(u32) == 0, "Command size must be DWORD-aligned");

            const auto* dwords = reinterpret_cast<const u32*>(&cmd);
            const usize count = sizeof(T) / sizeof(u32);
            for (usize i = 0; i < count; i++) {
                ring_write(dwords[i]);
            }
        }

        void ring_flush();
        [[nodiscard]] bool ring_wait_space(u32 required_bytes, u32 timeout_us) const;

        virtual void emit_flush(u32 seqno) = 0;

        /// Selects how submit_ring() hands work to hardware from here on. Switching to Execlist
        /// requires ensure_execlist_mode_enabled() and lrc_alloc_and_init() to have already been
        /// called for `ctx` (asserts otherwise in debug builds via the lrc_cpu_addr_ null check
        /// inside submit_ring()).
        void set_submission_mode(SubmissionMode mode) {
            submission_mode_ = mode;
        }

        [[nodiscard]] SubmissionMode submission_mode() const {
            return submission_mode_;
        }

        /// The one call every command-emitting helper should use instead of calling
        /// ring_flush() directly. Dispatches on submission_mode_:
        ///   - LegacyRing: identical to today's ring_flush() (pads to 8B, writes RING_BUFFER_TAIL).
        ///                 `ctx` is unused in this mode.
        ///   - Execlist:   pads to 8B like ring_flush(), but instead of touching RING_BUFFER_TAIL
        ///                 it rewrites LRC_DW_RING_TAIL inside `ctx`'s LRC and re-submits the
        ///                 execlist via lrc_submit(ctx). The ring buffer itself, ring_write(),
        ///                 ring_write_cmd() and ring_tail_ tracking are all shared unchanged
        ///                 between both modes — only *how the tail becomes visible to HW* differs,
        ///                 and now *which context's LRC* it becomes visible through.
        ///
        /// @note Only one context's execlist can be in flight on this engine's single physical
        /// ring at a time -- the caller (LucFile::exec_submit()) is responsible for serializing
        /// submissions across contexts on the same engine (see the locking TODO in
        /// intel_luc_file.h). This is what makes "multiple processes share one ring but each gets
        /// its own LRC" safe rather than a race.
        void submit_ring(EngineContext& ctx);

        void hwsp_alloc();
        u32 seqno_next();
        bool seqno_wait(u32 target_seqno, u32 timeout_us, AtomicFlag& completion_flag, const EngineContext& ctx);

        /// One-time, per-ENGINE (not per-context) hardware bring-up: applies the FORCE_TO_NONPRIV
        /// whitelist and flips GFX_MODE.execlist_enable, then waits for it to land. Idempotent --
        /// safe to call again on a second/third context's first submission; it just re-verifies
        /// and re-writes the same engine-global registers. Split out of the old lrc_alloc_and_init()
        /// because none of this is context state: it was only ever called once total (on the first
        /// and only context that engine ever had), so bundling it into "initialize a context" read
        /// as context setup when it's actually engine setup that happens to be triggered by the
        /// first context's arrival.
        ///
        /// @note Callers should call this before lrc_alloc_and_init() for a context's first use of
        /// this engine (LucFile::context_for_engine() does both together) -- lrc_alloc_and_init()
        /// itself no longer touches the whitelist or GFX_MODE at all.
        [[nodiscard]] bool ensure_execlist_mode_enabled();

        /// Allocates GGTT space for `ctx`'s LRC and writes its initial Logical Ring Context image
        /// (RCS register layout, RING_HEAD/TAIL/START/CTL, CONTEXT_CONTROL) -- pure per-context
        /// setup, no engine-global register writes. Called once per (LucFile, engine) pair, on
        /// that pair's first submission -- NOT once per engine. Deliberately close to a free
        /// function: only reads engine_mmio_offset_/ring_gfx_addr_/ring_size_/type_ and ggtt() from
        /// `this`, nothing engine-global is written. Requires ensure_execlist_mode_enabled() to
        /// have already succeeded once on this engine (LucFile::context_for_engine() enforces the
        /// order); this function does not check that itself.
        ///
        /// @param ctx Out-parameter: the context object being initialized. Must be default-constructed
        ///            (lrc_cpu_addr_ null) on entry; this is the one-time bring-up call for it.
        /// @param lrc_size_bytes LRC_SIZE_SMALL_ENGINE for BCS/VCS/VECS, LRC_SIZE_RCS for RCS.
        /// @param sw_context_id Software-assigned context ID (CONTEXT_DESCRIPTOR::sw_context_id),
        ///        unique per context sharing this engine -- see LucFile's context/handle allocation.
        ///
        /// @see CONTEXT_DESCRIPTOR::lrca
        [[nodiscard]] bool lrc_alloc_and_init(EngineContext& ctx, usize lrc_size_bytes, u32 sw_context_id) const;
        void lrc_free(const EngineContext& ctx) const;

        /// Rewrites just the Ring Tail DWord inside `ctx`'s already-initialized LRC, then submits
        /// an execlist with `ctx` as Element 0 (Element 1 left invalid). Element 1 valid=0 is
        /// written first per PRM-mandated submission order.
        void lrc_submit(const EngineContext& ctx) const;
        u32 read_seqno() const;
        void dump_ppgtt_page_faults() const;
        void log_lrc_context_image(const EngineContext& ctx) const;
        void log_pphwsp(const EngineContext& ctx) const;

        /// Rewrites just LRC_DW_RING_TAIL inside `ctx`'s already-initialized LRC to the given byte
        /// offset. Called by submit_ring() in Execlist mode instead of the RING_BUFFER_TAIL MMIO
        /// write that ring_flush() does in Legacy mode.
        void lrc_update_tail(const EngineContext& ctx, u32 tail_bytes) const;



        gfx_addr_t ring_gfx_addr_{};
        virt_addr_t ring_cpu_addr_{};
        phys_addr_t ring_phys_addr_{};
        u32 ring_size_ = 0;
        u32 ring_tail_ = 0;

        gfx_addr_t hwsp_gfx_addr_{};
        virt_addr_t hwsp_cpu_addr_{};
        phys_addr_t hwsp_phys_addr_{};
        u32 sequence_number_ = 0;

        u64 error_count_ = 0;
        AtomicFlag banned_{};

        SubmissionMode submission_mode_ = SubmissionMode::LegacyRing;

        bool execlist_mode_enabled_ = false;

        static constexpr u32 SEQNO_BIT5_MASK = 1u << 5;

    private:
        /// Writes one (MMIO-offset, value) pair into `ctx`'s Ring Context at the given LRC DWord offset
        void lrc_write_ring_field(const EngineContext& ctx, usize dword_offset, u32 engine_relative_mmio_off,
                                   u32 value) const;

        IntelGpuDevice& device_;
        u32 engine_mmio_offset_;
        ForceWakeDomain fw_domain_;
    };
} // namespace blt

#endif  // VESPERAOS_INTEL_ENGINE_H
