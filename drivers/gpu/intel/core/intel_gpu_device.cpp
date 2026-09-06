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

#include <gpu/intel/regs/gt_interrupt_regs.h>
#include <gpu/intel/regs/interrupt_regs.h>
#include "intel_engine.h"
#include "mocs_init.h"
#include "drivers/mmio_post_write.h"
#include "filesystem/devfs.h"
#include "gpu/intel/rcs/intel_rcs.h"
#include "gpu/intel/regs/fuse_regs.h"
#include "uapi/vespera/dev/lucifer_drm.h"

namespace gpu::intel::core {
    IntelGpuDevice::IntelGpuDevice(const pci::pci_device& igpu_dev)
        : CharDevice(BusType::Pci), igp_cfg_(reinterpret_cast<volatile INTEL_IGP_PCI_CONFIG*>(igpu_dev.header)),
          pci_id_(igpu_dev.id) {
        const phys_addr_t bar0 =
            make_phys(static_cast<u64>(igp_cfg_->gttmmadr_hi) << 32 | (igp_cfg_->gttmmadr_lo & GTTMMADR_ADDR_MASK));

        kernel::memory::map_range(phys_to_virt(bar0), bar0, BAR0_SIZE,
                                  (1ULL << CacheDisabled) | (1ULL << PtFlag::ReadWrite));

        mmio_base_ = static_cast<volatile u8*>(virt_ptr(phys_to_virt(bar0)));
    }

    bool IntelGpuDevice::init() {
        char name[16];
        DeviceManager::alloc_unique_device_name("dri/card", name, sizeof(name));
        kd_ = DeviceManager::register_device(
            DeviceDescriptor{}
            .set_name(name)
            .set_type(DeviceType::Gpu)
            .set_class(DeviceClass::Graphics)
            .set_bus(BusType::Pci)
            .set_controller(ControllerType::IntelGpu)
            .with_gpu(this)
            .with_info(this)
            .with_char(this)
        );

        DevFs::register_device(kd_);

        return ggtt_alloc_.init_from_device(mmio_base_, igp_cfg_, pci_id_);
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
            .eu_count       = 24
        };

        const ForceWakeDomain render_fw{
            rcs::FORCEWAKE_RENDER,
            rcs::FORCEWAKE_ACK_RENDER,
            rcs::FORCEWAKE_RENDER_ENABLE,
            FORCEWAKE_ACK_BIT,
            rcs::FORCEWAKE_RENDER_TIMEOUT
        };

        if (const bool fw_ok = force_wake_enable(render_fw); !fw_ok) {
            Log::warning("intel-gpu: ForceWake failed before reading FUSE2 registers, using default values!");
            return topo;
        }

        FUSE2 fuse2{};
        fuse2.raw = *reinterpret_cast<volatile u32*>(mmio_base_ + FUSE2_MMIO);

        if (fuse2.raw != 0x00000000u && fuse2.raw != 0xFFFFFFFFu) {
            u32 active_slices = __builtin_popcount(fuse2.slice_enable);
            if (active_slices > 0) {
                topo.slice_count = active_slices;
            }

            u32 disabled_subslices_per_slice = __builtin_popcount(fuse2.subslice_disable);
            u32 active_subslices_per_slice = (disabled_subslices_per_slice < 4)
                                                 ? (4 - disabled_subslices_per_slice)
                                                 : 0;

            topo.subslice_count = topo.slice_count * active_subslices_per_slice;

            // Gen9 standard: max 8 EUs per subslice
            constexpr u32 EUS_PER_SUBSLICE = 8;
            topo.eu_count = topo.subslice_count * EUS_PER_SUBSLICE;
        }

        return topo;
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

        // Lazily install the single shared MSI/MSI-X vector + device-level
        // dispatcher. Whichever caller gets here first (an engine via this
        // function, or the display path via register_de_pipe_a_handler())
        // actually wires up the hardware; subsequent calls just register
        // themselves with the dispatch tables below. allocate_irq_vector()
        // itself is idempotent (returns the existing vector if already
        // installed), so calling it again here is safe regardless of order.
        const u8 vec = allocate_irq_vector(reinterpret_cast<irq_handler_t>(device_irq_handler), this);
        if (vec == INVALID_VECTOR) {
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
        const u32 bit = (1u << engine->gt_user_irq_bit()) | engine->gt_debug_irq_bitmask();

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

                if (decoded.bits.rcs.master_error) {
                    Log::warning("intel-gpu: GT0 IIR RCS master_error pending");
                }
                if (decoded.bits.rcs.page_fault) {
                    Log::warning("intel-gpu: GT0 IIR RCS page_fault pending");
                }
                if (decoded.bits.rcs.timeout) {
                    Log::warning("intel-gpu: GT0 IIR RCS timeout pending");
                }
                if (decoded.bits.rcs.invalid_tile) {
                    Log::warning("intel-gpu: GT0 IIR RCS invalid_tile pending");
                }
                if (decoded.bits.rcs.ctx_switch) {
                    Log::debug("intel-gpu: GT0 IIR RCS ctx_switch pending");
                }
                if (decoded.bits.bcs.master_error) {
                    Log::warning("intel-gpu: GT0 IIR BCS master_error pending");
                }
                if (decoded.bits.bcs.timeout) {
                    Log::warning("intel-gpu: GT0 IIR BCS timeout pending");
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
        if (master.de_pipe_a_pending && self->de_pipe_a_handler_ != nullptr) {
            auto* iir_reg = reinterpret_cast<volatile u32*>(self->mmio_base_ + DE_PIPE_A_IIR);
            DE_PIPE_IIR iir{};
            iir.raw = *iir_reg;

            if (iir.vblank || iir.plane1_flip_done) {
                DE_PIPE_IIR clr{};
                clr.vblank = iir.vblank;
                clr.plane1_flip_done = iir.plane1_flip_done;
                *iir_reg = clr.raw;
                volatile u32 post_iir = *iir_reg;
                (void)post_iir;

                self->de_pipe_a_handler_(
                    self->de_pipe_a_handler_ctx_, iir.vblank != 0, iir.plane1_flip_done != 0
                );

                handled = true;
            }
        }

        master.master_enable = 1;
        *master_reg = master.raw;
        volatile u32 post = *master_reg;
        (void)post;

        return handled ? IRQ_HANDLED : IRQ_NONE;
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

    int IntelGpuDevice::ioctl(CharFile*, const u32 request, void* arg) {
        if (request == LUCIFER_IOCTL_VERSION) {
            auto* ver = static_cast<lucifer_version*>(arg);
            if (!ver) {
                return -1;
            }

            ver->version_major = 1;
            ver->version_minor = 0;
            ver->version_patchlevel = 0;

            strncpy(ver->name, "lucifer", sizeof(ver->name) - 1);
            ver->name[sizeof(ver->name) - 1] = '\0';

            strncpy(ver->date, "20260826", sizeof(ver->date) - 1);
            ver->date[sizeof(ver->date) - 1] = '\0';

            strncpy(ver->desc, "Lucifer DRM driver for Intel", sizeof(ver->desc) - 1);
            ver->desc[sizeof(ver->desc) - 1] = '\0';

            return 0;
        }

        if (request != LUCIFER_IOCTL_QUERY) {
            return -1;
        }

        auto* query = static_cast<lucifer_query*>(arg);
        if (!query) {
            return -1;
        }

        u32 expected_size = 0;

        switch (query->query) {
            case LUCIFER_QUERY_CONFIG:
                expected_size = sizeof(lucifer_query_config);
                break;
            case LUCIFER_QUERY_TOPOLOGY:
                expected_size = sizeof(lucifer_query_topology);
                break;
            case LUCIFER_QUERY_MEM_REGIONS:
                expected_size = sizeof(lucifer_query_mem_regions);
                break;
            case LUCIFER_QUERY_PCI_INFO:
                expected_size = sizeof(lucifer_query_pci_info);
                break;
            default:
                return -1;
        }

        if (query->data == 0) {
            query->size = expected_size;
            return 0;
        }

        if (query->size < expected_size) {
            return -1;
        }

        switch (query->query) {
            case LUCIFER_QUERY_CONFIG: {
                auto* out = reinterpret_cast<lucifer_query_config*>(query->data);

                out->device_id = igp_cfg_->device_id;
                out->revision = igp_cfg_->revision_id;

                FuseTopology topo = query_fuse_topology();
                if (topo.subslice_count >= 6) {
                    out->gt_level = 3; // GT3 / GT4
                } else if (topo.subslice_count >= 3) {
                    out->gt_level = 2; // GT2
                } else {
                    out->gt_level = 1; // GT1
                }

                out->gtt_size = ggtt_alloc_.usable_size_bytes();
                out->mem_alignment = 4096;
                out->timestamp_frequency = 12000000;

                out->pad0 = 0;
                out->pad1 = 0;
                out->pad2 = 0;
                break;
            }
            case LUCIFER_QUERY_TOPOLOGY: {
                auto* out = reinterpret_cast<lucifer_query_topology*>(query->data);

                FuseTopology topo = query_fuse_topology();

                out->slice_mask = (1u << topo.slice_count) - 1;

                u32 subs_per_slice = (topo.slice_count > 0) ? (topo.subslice_count / topo.slice_count) : 0;
                out->subslice_mask = (1u << subs_per_slice) - 1;

                u32 eus_per_sub = (topo.subslice_count > 0) ? (topo.eu_count / topo.subslice_count) : 0;
                out->eu_mask = (1u << eus_per_sub) - 1;

                if (topo.subslice_count >= 6) {
                    out->l3_banks = 4; // GT3 / GT4
                } else if (topo.subslice_count >= 2) {
                    out->l3_banks = 2; // GT2
                } else {
                    out->l3_banks = 1; // GT1
                }
                break;
            }
            case LUCIFER_QUERY_MEM_REGIONS: {
                auto* out = reinterpret_cast<lucifer_query_mem_regions*>(query->data);

                out->total_size = ggtt_alloc_.usable_size_bytes();

                const u64 used_pages = static_cast<u64>(ggtt_alloc_.persistent_used_pages()) +
                                       static_cast<u64>(ggtt_alloc_.transient_used_pages());

                out->used = used_pages * PAGE_SIZE;
                break;
            }
            case LUCIFER_QUERY_PCI_INFO: {
                auto* out = reinterpret_cast<lucifer_query_pci_info*>(query->data);

                out->domain = pci_id_.domain;
                out->bus = pci_id_.bus;
                out->dev = pci_id_.device;
                out->func = pci_id_.function;

                out->vendor_id = igp_cfg_->vendor_id;
                out->device_id = igp_cfg_->device_id;
                out->subsystem_vendor_id = igp_cfg_->subsystem_vendor_id;
                out->subsystem_device_id = igp_cfg_->subsystem_id;
                out->revision = igp_cfg_->revision_id;

                out->name[0] = '\0';
                if (kd_ && kd_->name) {
                    strncpy(out->name, kd_->name, sizeof(out->name) - 1);
                    out->name[sizeof(out->name) - 1] = '\0';
                }
                break;
            }
            default: ;
        }

        query->size = expected_size;

        return 0;
    }

    bool IntelGpuDevice::fill_rect(u32 px, u32 py, u32 w, u32 h, u32 colour) {
        return bcs_->fill_rect(px, py, w, h, colour);
    }

    bool IntelGpuDevice::blit_region(
        const u32* pixels, u32 src_stride, u32 src_x, u32 src_y, u32 w, u32 h, u32 dst_x, u32 dst_y
    ) {
        return bcs_->blit_region(pixels, src_stride, src_x, src_y, w, h, dst_x, dst_y);
    }

    void IntelGpuDevice::present() {
        return bcs_->present();
    }

    [[nodiscard]] u32 IntelGpuDevice::screen_width_px() const {
        return bcs_->screen_width_px();
    }

    [[nodiscard]] u32 IntelGpuDevice::screen_height_px() const {
        return bcs_->screen_height_px();
    }

    [[nodiscard]] u32 IntelGpuDevice::bytes_per_scanline() const {
        return bcs_->bytes_per_scanline();
    }
} // namespace blt
