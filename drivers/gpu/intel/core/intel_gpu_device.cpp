// intel_gpu_device.cpp
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

#include "intel_gpu_device.h"

#include <pci/msix.h>
#include <pci/pci_device.h>
#include <vespera/interrupts.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/time.h>

#include "intel_workarounds.h"
#include <gpu/intel/regs/gt_interrupt_regs.h>
#include <gpu/intel/regs/interrupt_regs.h>
#include "intel_engine.h"
#include "intel_luc_file.h"
#include "intel_ppgtt.h"
#include "drivers/mmio_post_write.h"
#include "filesystem/devfs.h"
#include "gpu/intel/regs/fuse_regs.h"
#include "intel_node_kind.h"
#include "gpu/intel/regs/gt_power_regs.h"
#include "gpu/intel/regs/mchbar_regs.h"
#include "pci/pci_host_bridge.h"
#include "vespera/graphics/display_manager.h"

// TODO: allocate_irq_vector dont lazy init

namespace {
    void render_name_from_card_name(const char* card_name, char* out, const usize out_len) {
        constexpr char CARD_PREFIX[] = "dri/card";
        constexpr char RENDER_PREFIX[] = "dri/renderD";

        u32 idx = 0;
        for (const char* p = card_name + (sizeof(CARD_PREFIX) - 1); *p >= '0' && *p <= '9'; ++p) {
            idx = idx * 10 + static_cast<u32>(*p - '0');
        }
        idx += 128;

        usize n = 0;
        for (; RENDER_PREFIX[n] != '\0' && n + 1 < out_len; ++n) {
            out[n] = RENDER_PREFIX[n];
        }

        char digits[10];
        usize d = 0;
        do {
            digits[d++] = static_cast<char>('0' + idx % 10);
            idx /= 10;
        }
        while (idx != 0 && d < sizeof(digits));

        while (d != 0 && n + 1 < out_len) {
            out[n++] = digits[--d];
        }
        out[n] = '\0';
    }
} // namespace

namespace gpu::intel::core {
    IntelGpuDevice::IntelGpuDevice(const pci::pci_device& igpu_dev)
        : CharDevice(BusType::Pci), igp_cfg_(reinterpret_cast<volatile INTEL_IGP_PCI_CONFIG*>(igpu_dev.header)),
          pci_id_(igpu_dev.id), display_(*this) {
        const phys_addr_t bar0 =
            make_phys(static_cast<u64>(igp_cfg_->gttmmadr_hi) << 32 | (igp_cfg_->gttmmadr_lo & GTTMMADR_ADDR_MASK));

        kernel::memory::map_range(
            phys_to_virt(bar0),
            bar0,
            BAR0_SIZE,
            (1ULL << PtFlag::ReadWrite),
            CacheMode::Uncacheable
        );

        mmio_base_ = static_cast<volatile u8*>(virt_ptr(phys_to_virt(bar0)));
    }

    bool IntelGpuDevice::init() {
        if (!init_private_pat()) {
            Log::error("intel-gpu: private PAT setup failed");
            return false;
        }

        if (!ggtt_alloc_.init_from_device(mmio_base_, igp_cfg_, pci_id_)) {
            return false;
        }

        const u8 vec = allocate_irq_vector(reinterpret_cast<irq_handler_t>(device_irq_handler), this);
        if (vec == INVALID_VECTOR) {
            Log::error("intel-gpu: failed to allocate device IRQ vector");
            return false;
        }

        fuse_topology_ = query_fuse_topology();
        gt_freq_limits_ = query_gt_frequency_limits();

        Log::debug(
            "intel-gpu: GT freq limits: min=%u MHz, guaranteed=%u MHz, max=%u MHz, effective_max=%u MHz (capped=%s)",
            gt_freq_limits_.min_mhz,
            gt_freq_limits_.guaranteed_mhz,
            gt_freq_limits_.max_mhz,
            gt_freq_limits_.effective_max_mhz,
            gt_freq_limits_.is_capped ? "yes" : "no");

        Log::debug("intel-gpu: GT current freq: %u Mhz", current_gt_freq_mhz());

        const u32 target_freq = (gt_freq_limits_.valid && gt_freq_limits_.effective_max_mhz > 0)
                                    ? gt_freq_limits_.effective_max_mhz
                                    : 500u;
        request_gt_frequency(500);
        kernel::time::sleep_ms(10);
        Log::debug("intel-gpu: GT current freq: %u Mhz (updated)", current_gt_freq_mhz());

        wa::enable_all_mmio_workarounds(mmio_base(), fuse_topology_.mcr_selector);

        if (!display_.init()) {
            Log::error("intel-gpu: failed to initialize display subsystem");
            return false;
        }

        master_int_ctl_enable();

        char name[16];
        DeviceManager::alloc_unique_device_name("dri/card", name, sizeof(name));
        kd_ = DeviceManager::register_device(
            DeviceDescriptor{}
            .set_name(name)
            .set_type(DeviceType::Char)
            .set_class(DeviceClass::Graphics)
            .set_bus(BusType::Pci)
            .set_controller(ControllerType::IntelGpu)
            .with_gpu(&this->display_)
            .with_info(this)
            .with_char(this)
        );

        DevFs::register_device(kd_);

        char render_name[24];
        render_name_from_card_name(name, render_name, sizeof(render_name));
        render_kd_ = DeviceManager::register_device(
            DeviceDescriptor{}
            .set_name(render_name)
            .set_type(DeviceType::Char)
            .set_class(DeviceClass::Graphics)
            .set_bus(BusType::Pci)
            .set_controller(ControllerType::IntelGpu)
            .with_info(this)
            .with_char(this)
        );

        DevFs::register_device(render_kd_);

        kernel::time::sleep_ms(2000);

        DisplayManager::set_primary(DisplayBackend{&display_, kd_});
        return true;
    }

    bool IntelGpuDevice::force_wake_enable(const ForceWakeDomain& domain) const {
        volatile auto* fw_set = reinterpret_cast<volatile u32*>(mmio_base_ + domain.request_reg);
        const volatile auto* fw_ack = reinterpret_cast<volatile u32*>(mmio_base_ + domain.ack_reg);

        *fw_set = domain.enable_value;

        u32 timeout = domain.timeout_us;

        while (timeout--) {
            if (*fw_ack & domain.ack_bit) {
                return true;
            }

            kernel::time::sleep_us(1);
        }

        Log::log_dbc(
            "intel-gpu: ForceWake timeout (request=0x%x ack=0x%x)", domain.request_reg, domain.ack_reg
        );
        return false;
    }

    IntelGpuDevice::FuseTopology IntelGpuDevice::query_fuse_topology() const {
        FuseTopology topo = {
            .slice_count    = 1,
            .subslice_count = 3,
            .eu_count       = 24,
            .mcr_selector   = 0
        };

        if (!force_wake_enable(RENDER_FORCEWAKE_DOMAIN)) {
            Log::warning("intel-gpu: ForceWake failed before reading FUSE2 registers, using default values!");
            return topo;
        }

        FUSE2 fuse2{};
        fuse2.raw = *reinterpret_cast<volatile u32*>(mmio_base_ + FUSE2_MMIO);

        if (fuse2.raw != 0x00000000u && fuse2.raw != 0xFFFFFFFFu) {
            const u32 slice_mask = fuse2.slice_enable & 0x7u;
            u32 active_slices = __builtin_popcount(fuse2.slice_enable);
            if (active_slices > 0) {
                topo.slice_count = active_slices;
            }

            const u32 subslice_disable_mask = fuse2.subslice_disable & 0xFu;
            u32 disabled_subslices_per_slice = __builtin_popcount(fuse2.subslice_disable);
            u32 active_subslices_per_slice = (disabled_subslices_per_slice < 4)
                                                 ? (4 - disabled_subslices_per_slice)
                                                 : 0;

            topo.subslice_count = topo.slice_count * active_subslices_per_slice;

            // Gen9 standard: max 8 EUs per subslice
            constexpr u32 EUS_PER_SUBSLICE = 8;
            topo.eu_count = topo.subslice_count * EUS_PER_SUBSLICE;

            const u32 subslice_enable_mask = (~subslice_disable_mask) & 0xFu;
            const u32 first_slice = slice_mask ? static_cast<u32>(__builtin_ctz(slice_mask)) : 0u;
            const u32 first_subslice = subslice_enable_mask
                                           ? static_cast<u32>(__builtin_ctz(subslice_enable_mask))
                                           : 0u;
            topo.mcr_selector = (first_slice << 24) | (first_subslice << 21);
        }

        return topo;
    }

    IntelGpuDevice::GtFrequencyLimits IntelGpuDevice::query_gt_frequency_limits() {
        GtFrequencyLimits limits{};

        const volatile pci::INTEL_HB_PCI_CONFIG* hb_cfg = pci::get_host_bridge(pci_id_.domain);
        if (!hb_cfg) {
            Log::error("intel-gpu: host bridge config space not available for MCHBAR lookup");
            return limits;
        }

        MCHBAR_BASE_0_0_0_PCI mchbar_reg{};
        mchbar_reg.raw = hb_cfg->mchbar;

        if (!mchbar_reg.enable) {
            Log::warning("intel-gpu: MCHBAR not enabled by firmware, cannot read GT frequency caps");
            return limits;
        }

        const phys_addr_t mchbar_phys = make_phys(mchbar_reg.base_address());

        if (!mchbar_base_) {
            constexpr usize MCHBAR_WINDOW_SIZE = 0x8000;

            kernel::memory::map_range(
                phys_to_virt(mchbar_phys),
                mchbar_phys,
                MCHBAR_WINDOW_SIZE,
                (1ULL << PtFlag::ReadWrite),
                CacheMode::Uncacheable
            );

            mchbar_base_ = static_cast<volatile u8*>(virt_ptr(phys_to_virt(mchbar_phys)));
        }

        regs::RP_STATE_CAP_MCHBAR cap{};
        cap.raw = *reinterpret_cast<volatile u32*>(mchbar_base_ + regs::RP_STATE_CAP_MCHBAR_OFFSET);

        if (cap.raw == 0x00000000u || cap.raw == 0xFFFFFFFFu) {
            Log::warning("intel-gpu: RP_STATE_CAP read returned sentinel value, MCHBAR mapping may be wrong");
            return limits;
        }

        limits.min_mhz = cap.min_freq_mhz();
        limits.guaranteed_mhz = cap.guaranteed_freq_mhz();
        limits.max_mhz = cap.max_freq_mhz();

        regs::RP_STATE_LIMITS_MCHBAR sw_lim{};
        sw_lim.raw = *reinterpret_cast<volatile u32*>(mchbar_base_ + regs::RP_STATE_LIMITS_MCHBAR_OFFSET);


        if (sw_lim.raw != 0xFFFFFFFFu && sw_lim.has_limit() && sw_lim.limit_freq_mhz() < limits.max_mhz) {
            limits.effective_max_mhz = sw_lim.limit_freq_mhz();
            limits.is_capped = true;
        } else {
            limits.effective_max_mhz = limits.max_mhz;
            limits.is_capped = false;
        }

        limits.valid = true;

        return limits;
    }

    u32 IntelGpuDevice::current_gt_freq_mhz() const {
        GEN6_RPSTAT1 status{};
        status.raw = *reinterpret_cast<volatile u32*>(mmio_base_ + GEN6_RPSTAT1_OFFSET);

        return status.gen9_freq_mhz();
    }

    void IntelGpuDevice::request_gt_frequency(const u32 target_mhz) const {
        if (!force_wake_enable(RENDER_FORCEWAKE_DOMAIN)) {
            Log::warning("intel-gpu: ForceWake failed before RPNSWREQ write, skipping frequency request");
            return;
        }

        const GEN6_RPNSWREQ req = GEN6_RPNSWREQ::make_gen9(target_mhz);
        *reinterpret_cast<volatile u32*>(mmio_base_ + GEN6_RPNSWREQ_OFFSET) = req.raw;
        (void)*reinterpret_cast<volatile u32*>(mmio_base_ + GEN6_RPNSWREQ_OFFSET);

        Log::debug("intel-gpu: requested GT freq %u MHz via RPNSWREQ (raw=0x%08x)", target_mhz, req.raw);
    }

    u8 IntelGpuDevice::allocate_irq_vector(irq_handler_t handler, void* ctx) {
        if (irq_vector_ != INVALID_VECTOR) {
            return irq_vector_;
        }

        const u8 vec = kernel::interrupts::get_free_vector();
        kernel::interrupts::allocate_vector(vec, handler, ctx);

        if (!pci::try_enable_msi_or_msix(reinterpret_cast<volatile pci::PCI_HEADER0*>(igp_cfg_), vec, 1)) {
            Log::log_dbc("intel-gpu: MSI enable failed");
            return INVALID_VECTOR;
        }

        irq_vector_ = vec;
        return irq_vector_;
    }

    void IntelGpuDevice::master_int_ctl_enable() const {
        auto* reg = reinterpret_cast<volatile u32*>(mmio_base_ + GEN8_MASTER_INT_CTL_OFFSET);
        MASTER_INT_CTL master{};
        master.raw = *reg;
        master.master_enable = 1;
        *reg = master.raw;
        volatile u32 post = *reg;
        (void)post;
    }

    bool IntelGpuDevice::register_engine_for_irq(IntelEngine* engine) {
        if (gt_irq_engine_count_ >= MAX_GT_IRQ_ENGINES) {
            Log::log_dbc("intel-gpu: GT IRQ engine registry full");
            return false;
        }

        gt_irq_engines_[gt_irq_engine_count_++] = engine;

        // Unmask + enable this engine's completion bit (gt_user_irq_bit())
        // AND any additional debug-only bits it wants visible
        // (gt_debug_irq_bitmask() — e.g. RCS unmasking page_fault/
        // master_error purely so device_irq_handler() can log them, even
        // though those bits don't drive on_gt_user_interrupt()). Read-
        // modify-write via raw is required here — GT0_IMR/IER is the one
        // register two engines legitimately share (RCS bits [15:0], BCS
        // bits [31:16]); a naive full-register write would clobber
        // whichever other engine already registered.
        auto* gt0 = reinterpret_cast<volatile GT_INTR_REGS*>(mmio_base_ + GEN8_GT0_INTR_BASE);
        const u32 bit = engine->gt_unmask_bits() | engine->gt_debug_irq_bitmask();

        u32 imr = gt0->imr.raw;
        imr &= ~bit; // 0 = unmasked
        gt0->imr.raw = imr;
        MMIO_POST_WRITE(gt0->imr);

        // Clear any stale pending bit for this engine before enabling —
        // IIR is W1C, so writing just this bit clears only it. Must land
        // before IER enables the bit below, or a stale pending state from
        // before this engine registered could immediately re-fire.
        gt0->iir.raw = bit;
        MMIO_POST_WRITE(gt0->iir);

        u32 ier = gt0->ier.raw;
        ier |= bit; // 1 = enabled
        gt0->ier.raw = ier;
        MMIO_POST_WRITE(gt0->ier);

        master_int_ctl_enable();

        return true;
    }

    /**
     * @deprecated
     */
    bool IntelGpuDevice::register_de_pipe_a_handler(DePipeAHandler handler, void* ctx) {
        const u8 vec = allocate_irq_vector(reinterpret_cast<irq_handler_t>(device_irq_handler), this);
        if (vec == INVALID_VECTOR) {
            return false;
        }

        de_pipe_a_handler_ = handler;
        de_pipe_a_handler_ctx_ = ctx;

        DE_PIPE_IIR clr{};
        clr.vblank = 1;
        clr.plane1_flip_done = 1;
        auto* iir = reinterpret_cast<volatile u32*>(mmio_base_ + DE_PIPE_A_IIR);
        *iir = clr.raw;

        DE_PIPE_IMR imr{};
        imr.raw = *reinterpret_cast<volatile u32*>(mmio_base_ + DE_PIPE_A_IMR);
        imr.vblank = 1;           // masked until armed
        imr.plane1_flip_done = 0; // unmasked
        *reinterpret_cast<volatile u32*>(mmio_base_ + DE_PIPE_A_IMR) = imr.raw;

        DE_PIPE_IER ier{};
        ier.raw = *reinterpret_cast<volatile u32*>(mmio_base_ + DE_PIPE_A_IER);
        ier.vblank = 0;
        ier.plane1_flip_done = 1;
        *reinterpret_cast<volatile u32*>(mmio_base_ + DE_PIPE_A_IER) = ier.raw;

        master_int_ctl_enable();

        return true;
    }

    void IntelGpuDevice::de_pipe_a_arm_vblank_oneshot() const {
        auto* imr_reg = reinterpret_cast<volatile u32*>(mmio_base_ + DE_PIPE_A_IMR);
        DE_PIPE_IMR imr{};
        imr.raw = *imr_reg;
        imr.vblank = 0; // unmask
        *imr_reg = imr.raw;

        auto* ier_reg = reinterpret_cast<volatile u32*>(mmio_base_ + DE_PIPE_A_IER);
        DE_PIPE_IER ier{};
        ier.raw = *ier_reg;
        ier.vblank = 1; // enable
        *ier_reg = ier.raw;
        volatile u32 post = *ier_reg;
        (void)post;
    }

    void IntelGpuDevice::de_pipe_a_disarm_vblank() const {
        auto* imr_reg = reinterpret_cast<volatile u32*>(mmio_base_ + DE_PIPE_A_IMR);
        DE_PIPE_IMR imr{};
        imr.raw = *imr_reg;
        imr.vblank = 1; // mask (1 = OFF)
        *imr_reg = imr.raw;

        auto* ier_reg = reinterpret_cast<volatile u32*>(mmio_base_ + DE_PIPE_A_IER);
        DE_PIPE_IER ier{};
        ier.raw = *ier_reg;
        ier.vblank = 0; // disable (0 = OFF)
        *ier_reg = ier.raw;
        volatile u32 post = *ier_reg;
        (void)post;
    }

    Irqreturn IntelGpuDevice::device_irq_handler(IntelGpuDevice* self) {
        auto* master_reg = reinterpret_cast<volatile u32*>(self->mmio_base_ + GEN8_MASTER_INT_CTL_OFFSET);
        MASTER_INT_CTL master{};
        master.raw = *master_reg;
        if (!master.master_enable) {
            return IRQ_NONE;
        }

        *master_reg = 0;
        volatile u32 post_dis = *master_reg;
        (void)post_dis;

        bool handled = false;

        // ---- GT0 path (RCS + BCS) ----
        if (master.render_pending || master.blitter_pending) {
            auto* gt0 = reinterpret_cast<volatile GT_INTR_REGS*>(self->mmio_base_ + GEN8_GT0_INTR_BASE);
            const u32 pending = gt0->iir.raw;

            if (pending != 0) {
                // W1C: clear exactly the bits observed as pending — never
                // read-modify-write-back a value that could re-arm or miss a
                // bit set between our read and this write. The read-back
                // afterwards isn't optional here: we're about to hand off to
                // engine->on_gt_user_interrupt(), and without forcing the
                // clear to actually land first, a slow write could still be
                // in flight when we re-enable routing below.
                gt0->iir.raw = pending;
                volatile u32 post_iir = gt0->iir.raw;
                (void)post_iir;

                GT0_IIR_REG decoded{};
                decoded.raw = pending;

                IntelEngine* rcs = self->engine_for_class(LUCIFER_ENGINE_CLASS_RENDER);
                IntelEngine* bcs = self->engine_for_class(LUCIFER_ENGINE_CLASS_COPY);

                // master_error/timeout are fatal for the engine's current
                // batch -- the seqno this engine is working towards will
                // never retire, so mark it banned here. This is the single
                // point where a hardware fault becomes visible to anyone
                // blocked in IntelEngine::seqno_wait()/seqno_wait_blocking()
                // or polling via syncobj_wait(), which is what eventually
                // lets Mesa/iris surface it as a GL error instead of
                // hanging forever on a fence that can't signal.
                if (decoded.bits.rcs.master_error) {
                    Log::warning("intel-gpu: GT0 IIR RCS master_error pending");
                    // rcs->debug_dump_error_regs("master error");
                    //rcs->dump_error_state("master error");
                    rcs->mark_banned();
                }
                if (decoded.bits.rcs.page_fault) {
                    Log::warning("intel-gpu: GT0 IIR RCS page_fault pending");
                }
                if (decoded.bits.rcs.timeout) {
                    Log::warning("intel-gpu: GT0 IIR RCS timeout pending");
                    rcs->mark_banned();
                }
                if (decoded.bits.rcs.invalid_tile) {
                    Log::warning("intel-gpu: GT0 IIR RCS invalid_tile pending");
                }
                if (decoded.bits.rcs.ctx_switch) {
                    rcs->on_gt_context_switch();
                }
                if (decoded.bits.bcs.master_error) {
                    Log::warning("intel-gpu: GT0 IIR BCS master_error pending");
                    bcs->mark_banned();
                }
                if (decoded.bits.bcs.timeout) {
                    Log::warning("intel-gpu: GT0 IIR BCS timeout pending");
                    bcs->mark_banned();
                }

                for (usize i = 0; i < self->gt_irq_engine_count_; i++) {
                    IntelEngine* engine = self->gt_irq_engines_[i];
                    const u32 bit = 1u << engine->gt_user_irq_bit();

                    if (pending & bit) {
                        engine->on_gt_user_interrupt();
                    }
                }

                handled = true;
            }
        }

        // ---- DE Pipe A path (display vblank / flip-done) ----
        if (self->display_.handle_irq(master)) {
            handled = true;
        }

        MASTER_INT_CTL reenable{};
        reenable.master_enable = 1;
        *master_reg = reenable.raw;
        volatile u32 post = *master_reg;
        (void)post;

        return handled ? IRQ_HANDLED : IRQ_NONE;
    }

    constexpr u32 PAT_INDEX_L = 0x40E0;
    constexpr u32 PAT_INDEX_H = 0x40E4;

    bool IntelGpuDevice::init_private_pat() const {
        if (!force_wake_enable(RENDER_FORCEWAKE_DOMAIN)) return false;

        auto rd = [&](u32 off) { return *reinterpret_cast<volatile u32*>(mmio_base_ + off); };
        auto wr = [&](u32 off, u32 v) { *reinterpret_cast<volatile u32*>(mmio_base_ + off) = v; };

        constexpr u32 PAT_L = 0x40E0, PAT_H = 0x40E4;
        constexpr u32 LO = 0x000A0907u, HI = 0x3B2B1B0Bu;

        Log::info("intel-pat: before lo=%08x hi=%08x", rd(PAT_L), rd(PAT_H));
        wr(PAT_L, LO);
        wr(PAT_H, HI);
        const u32 lo = rd(PAT_L), hi = rd(PAT_H);
        Log::info("intel-pat: after  lo=%08x hi=%08x", lo, hi);

        return lo == LO && hi == HI;
    }

    bool IntelGpuDevice::get_vendor(char* out, const usize len) {
        strncpy(out, pci::get_vendor_name(igp_cfg_->vendor_id), len);
        out[len - 1] = '\0';
        return true;
    }

    bool IntelGpuDevice::get_model(char* out, const usize len) {
        strncpy(out, pci::get_device_name(igp_cfg_->vendor_id, igp_cfg_->device_id), len);
        out[len - 1] = '\0';
        return true;
    }

    IntelEngine* IntelGpuDevice::engine_for_class(const u32 engine_class) const {
        if (engine_class >= LUCIFER_NUM_ENGINE_CLASSES) {
            return nullptr;
        }
        return engines_[engine_class];
    }

    void IntelGpuDevice::engine_signal_seqno(const u32 engine_class) {
        if (engine_class >= LUCIFER_NUM_ENGINE_CLASSES) {
            return;
        }

        // The HWSP seqno itself already advanced (hardware wrote it before
        // raising the completion interrupt) -- this call exists purely to
        // wake whoever is parked in engine_waiters_[engine_class] so they
        // re-check IntelEngine::seqno_ptr_for_read() themselves. Runs in
        // IRQ context, hence wake_all_irq() rather than wake_all().
        engine_waiters_[engine_class].wake_all_irq();
    }

    void IntelGpuDevice::note_submit(const u32 engine_class, const EngineContext* ctx, const u32 seqno) {
        if (engine_class >= LUCIFER_NUM_ENGINE_CLASSES || !ctx) return;
        SpinlockGuard guard(last_submit_lock_);
        LastSubmit& e = last_submit_[engine_class];
        // Engine seqnos are one monotonic counter per engine, so keep the newest.
        // Assumes in-order completion per engine (true for your single ring/execlist).
        if (!e.ctx || static_cast<i32>(seqno - e.seqno) > 0) {
            e = LastSubmit{ctx, seqno};
        }
    }

    void IntelGpuDevice::forget_submit_ctx(const EngineContext* ctx) {
        SpinlockGuard guard(last_submit_lock_);
        for (auto& e : last_submit_) {
            if (e.ctx == ctx) e = LastSubmit{};
        }
    }

    int IntelGpuDevice::count_pending_submits() const {
        SpinlockGuard guard(last_submit_lock_);
        int pending = 0;
        for (u32 i = 0; i < LUCIFER_NUM_ENGINE_CLASSES; ++i) {
            const LastSubmit& e = last_submit_[i];
            if (!e.ctx) continue;

            IntelEngine* engine = engine_for_class(i);
            if (!engine) continue;
            if (engine->is_banned()) return -EIO;

            const u32 done = __atomic_load_n(engine->seqno_ptr_for_read(*e.ctx), __ATOMIC_ACQUIRE);
            if (static_cast<i32>(done - e.seqno) < 0) pending++;
        }
        return pending;
    }

    bool IntelGpuDevice::wait_seqno(
        const u32 engine_class, const EngineContext* ctx, const u32 target_seqno, const i64 timeout_ns
    ) const {
        if (!ctx) return true;
        const IntelEngine* engine = engine_for_class(engine_class);
        return engine ? engine->wait_seqno(*ctx, target_seqno, timeout_ns) : false;
    }

    int IntelGpuDevice::open(CharFile** out_cf) {
        if (!out_cf || !*out_cf) {
            return -EINVAL;
        }

        CharFile* cf = *out_cf;
        const NodeKind kind =
            (cf->node != nullptr && cf->node == render_kd_) ? NodeKind::Render : NodeKind::Primary;

        cf->driver_private = new LucFile(*this, next_luc_file_id_++, kind);
        return 0;
    }

    int IntelGpuDevice::release(CharFile* cf) {
        delete static_cast<LucFile*>(cf->driver_private);
        cf->driver_private = nullptr;
        return 0;
    }

    int IntelGpuDevice::ioctl(CharFile* cf, const u32 request, void* arg) {
        return static_cast<LucFile*>(cf->driver_private)->ioctl(request, arg);
    }

    kernel::vm::VmBackingObject* IntelGpuDevice::get_backing_object(CharFile* cf, const u64 offset) {
        return static_cast<LucFile*>(cf->driver_private)->get_backing_object(offset);
    }
} // namespace gpu::intel::core
