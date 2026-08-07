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

#include "gt_interrupt_regs.h"
#include "intel_engine.h"
#include "interrupt_regs.h"
#include "drivers/mmio_post_write.h"

namespace blt {

    IntelGpuDevice::IntelGpuDevice(const pci::pci_device& igpu_dev)
        : igp_cfg_(reinterpret_cast<volatile INTEL_IGP_PCI_CONFIG*>(igpu_dev.header)), pci_id_(igpu_dev.id) {
        const phys_addr_t bar0 =
            make_phys(static_cast<u64>(igp_cfg_->gttmmadr_hi) << 32 | (igp_cfg_->gttmmadr_lo & GTTMMADR_ADDR_MASK));

        kernel::memory::map_range(phys_to_virt(bar0), bar0, BAR0_SIZE, (1ULL << CacheDisabled));

        mmio_base_ = static_cast<volatile u8*>(virt_ptr(phys_to_virt(bar0)));
    }

    bool IntelGpuDevice::init() {
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

        const u8 vec = allocate_irq_vector(reinterpret_cast<irq_handler_t>(device_irq_handler), this);
        if (vec == INVALID_VECTOR) {
            return false;
        }

        gt_irq_engines_[gt_irq_engine_count_++] = engine;

        auto* gt0 = reinterpret_cast<volatile GT_INTR_REGS*>(mmio_base_ + GEN8_GT0_INTR_BASE);
        const u32 bit = 1u << engine->gt_user_irq_bit();

        u32 imr = gt0->imr.raw;
        imr &= ~bit;  // 0 = unmasked
        gt0->imr.raw = imr;
        MMIO_POST_WRITE(gt0->imr);

        // Clear any stale pending bit for this engine before enabling —
        // IIR is W1C, so writing just this bit clears only it.
        gt0->iir.raw = bit;
        MMIO_POST_WRITE(gt0->iir);

        u32 ier = gt0->ier.raw;
        ier |= bit;  // 1 = enabled
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
        imr.vblank = 1;             // masked until armed
        imr.plane1_flip_done = 0;   // unmasked
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
        imr.vblank = 0;  // unmask
        *imr_reg = imr.raw;

        auto* ier_reg = reinterpret_cast<volatile u32*>(mmio_base_ + DE_PIPE_A_IER);
        DE_PIPE_IER ier{};
        ier.raw = *ier_reg;
        ier.vblank = 1;  // enable
        *ier_reg = ier.raw;
        volatile u32 post = *ier_reg;
        (void)post;
    }

    void IntelGpuDevice::de_pipe_a_disarm_vblank() const {
        auto* imr_reg = reinterpret_cast<volatile u32*>(mmio_base_ + DE_PIPE_A_IMR);
        DE_PIPE_IMR imr{};
        imr.raw = *imr_reg;
        imr.vblank = 1;  // mask (1 = OFF)
        *imr_reg = imr.raw;

        auto* ier_reg = reinterpret_cast<volatile u32*>(mmio_base_ + DE_PIPE_A_IER);
        DE_PIPE_IER ier{};
        ier.raw = *ier_reg;
        ier.vblank = 0;  // disable (0 = OFF)
        *ier_reg = ier.raw;
        volatile u32 post = *ier_reg;
        (void)post;
    }

    Irqreturn IntelGpuDevice::device_irq_handler(IntelGpuDevice* self) {
        auto* master_reg = reinterpret_cast<volatile u32*>(self->mmio_base_ + GEN8_MASTER_INT_CTL_OFFSET);
        MASTER_INT_CTL master{};
        master.raw = *master_reg;

        bool handled = false;

        if (master.render_pending || master.blitter_pending) {
            auto* gt0 = reinterpret_cast<volatile GT_INTR_REGS*>(self->mmio_base_ + GEN8_GT0_INTR_BASE);
            const u32 pending = gt0->iir.raw;

            if (pending != 0) {
                // W1C: clear exactly the bits observed as pending — never
                // read-modify-write-back a value that could re-arm or miss a
                // bit set between our read and this write.
                gt0->iir.raw = pending;

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

}  // namespace blt
