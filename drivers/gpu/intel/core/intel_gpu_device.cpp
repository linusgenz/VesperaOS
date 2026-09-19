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
#include <errno.h>

#include <gpu/intel/regs/gt_interrupt_regs.h>
#include <gpu/intel/regs/interrupt_regs.h>
#include "intel_engine.h"
#include "intel_ppgtt.h"
#include "drivers/mmio_post_write.h"
#include "filesystem/devfs.h"
#include "gpu/intel/rcs/intel_rcs.h"
#include "gpu/intel/bcs/intel_bcs.h"
#include "gpu/intel/regs/fuse_regs.h"
#include "intel_gem_backing.h"
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
            .set_type(DeviceType::Char)
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
                    self->rcs_->debug_dump_error_regs("master error");
                    self->rcs_->dump_error_state("master error");
                    self->rcs_->mark_banned();
                }
                if (decoded.bits.rcs.page_fault) {
                    Log::warning("intel-gpu: GT0 IIR RCS page_fault pending");
                }
                if (decoded.bits.rcs.timeout) {
                    Log::warning("intel-gpu: GT0 IIR RCS timeout pending");
                    self->rcs_->mark_banned();
                }
                if (decoded.bits.rcs.invalid_tile) {
                    Log::warning("intel-gpu: GT0 IIR RCS invalid_tile pending");
                }
                if (decoded.bits.rcs.ctx_switch) {
                    Log::debug("intel-gpu: GT0 IIR RCS ctx_switch pending");
                }
                if (decoded.bits.bcs.master_error) {
                    Log::warning("intel-gpu: GT0 IIR BCS master_error pending");
                    self->bcs_->mark_banned();
                }
                if (decoded.bits.bcs.timeout) {
                    Log::warning("intel-gpu: GT0 IIR BCS timeout pending");
                    self->bcs_->mark_banned();
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

    u32 IntelGpuDevice::create_vm() {
        for (usize i = 0; i < MAX_LUCIFER_VMS; ++i) {
            if (vm_slots_[i] != nullptr) {
                continue;
            }

            auto* ppgtt = new IntelPpgtt(ggtt_alloc_);
            if (!ppgtt->init()) {
                delete ppgtt;
                Log::log_dbc("intel-gpu: VM_CREATE failed (IntelPpgtt::init)");
                return 0;
            }

            vm_slots_[i] = ppgtt;
            return static_cast<u32>(i) + 1;
        }

        Log::log_dbc("intel-gpu: VM_CREATE failed (no free VM slots)");
        return 0;
    }

    bool IntelGpuDevice::destroy_vm(const u32 vm_id) {
        if (vm_id == 0 || vm_id > MAX_LUCIFER_VMS) {
            return false;
        }

        const usize slot = vm_id - 1;
        if (vm_slots_[slot] == nullptr) {
            return false;
        }

        delete vm_slots_[slot];
        vm_slots_[slot] = nullptr;
        return true;
    }

    IntelPpgtt* IntelGpuDevice::lookup_vm(const u32 vm_id) const {
        if (vm_id == 0 || vm_id > MAX_LUCIFER_VMS) {
            return nullptr;
        }
        return vm_slots_[vm_id - 1];
    }

    u32 IntelGpuDevice::gem_create(const lucifer_gem_create& args) {
        const usize page_count = (args.size + PAGE_SIZE - 1) / PAGE_SIZE;
        const phys_addr_t phys = kernel::memory::request_pages_phys(page_count);
        if (phys_null(phys)) {
            Log::log_dbc("intel-gpu: GEM_CREATE failed (request_pages_phys)");
            return 0;
        }

        memset(phys_to_virt(phys), 0, PAGE_SIZE * page_count);
        asm volatile("mfence" ::: "memory");

        for (usize i = 0; i < MAX_LUCIFER_GEM_OBJECTS; ++i) {
            if (gem_slots_[i].size != 0 || gem_slots_[i].is_userptr) {
                continue;
            }

            gem_slots_[i] = LucGemObject{
                .size = args.size,
                .placement = args.placement,
                .flags = args.flags,
                .cpu_caching = args.cpu_caching,
                .is_userptr = false,
                .userptr = 0,
                .phys_addr = phys,
            };

            return static_cast<u32>(i) + 1;
        }

        Log::log_dbc("intel-gpu: GEM_CREATE failed (no free GEM slots)");
        return 0;
    }

    u32 IntelGpuDevice::gem_create_userptr(const lucifer_gem_userptr& args) {
        for (usize i = 0; i < MAX_LUCIFER_GEM_OBJECTS; ++i) {
            if (gem_slots_[i].size != 0 || gem_slots_[i].is_userptr) {
                continue;
            }

            gem_slots_[i] = LucGemObject{
                .size = args.size,
                .placement = 0,
                .flags = 0,
                .cpu_caching = LUCIFER_GEM_CPU_CACHING_WB,
                .is_userptr = true,
                .userptr = args.ptr,
            };

            return static_cast<u32>(i) + 1;
        }

        Log::log_dbc("intel-gpu: GEM_USERPTR failed (no free GEM slots)");
        return 0;
    }

    bool IntelGpuDevice::gem_close(const u32 handle) {
        LucGemObject* obj = lookup_gem(handle);
        if (!obj) {
            return false;
        }

        // purged objects already returned their pages in gem_madvise();
        // nothing left to free here beyond the slot itself.
        if (!obj->is_userptr && !obj->purged) {
            const usize page_count = (obj->size + PAGE_SIZE - 1) / PAGE_SIZE;
            kernel::memory::free_pages_phys(obj->phys_addr, page_count);
        }

        *obj = LucGemObject{};
        return true;
    }

    bool IntelGpuDevice::gem_madvise(const lucifer_gem_madvise& args, bool* out_retained) {
        LucGemObject* obj = lookup_gem(args.handle);
        if (!obj || obj->is_userptr) {
            Log::log_dbc("intel-gpu: GEM_MADVISE failed (bad handle)");
            return false;
        }

        if (args.state == LUCIFER_MADVICE_DONT_NEED) {
            if (!obj->purged) {
                // Bring-up assumption: caller has already VM_BIND UNMAP'd
                // this object on every VM it was bound to -- see the
                // `purged` field comment on LucGemObject. Not verified
                // here (no back-reference from GEM object to bindings).
                const usize page_count = (obj->size + PAGE_SIZE - 1) / PAGE_SIZE;
                kernel::memory::free_pages_phys(obj->phys_addr, page_count);
                obj->phys_addr = phys_addr_t{};
                obj->purged = true;
            }

            *out_retained = false; // DONTNEED never reports resident
            return true;
        }

        // LUCIFER_MADVICE_WILL_NEED
        if (obj->purged) {
            // Backing store is gone and this bring-up path never
            // reallocates it -- caller (lucifer_bo_madvise()) is expected
            // to see retained == false and recreate the BO.
            *out_retained = false;
            return true;
        }

        *out_retained = true; // was never purged, still live
        return true;
    }

    IntelGpuDevice::LucGemObject* IntelGpuDevice::lookup_gem(const u32 handle) {
        if (handle == 0 || handle > MAX_LUCIFER_GEM_OBJECTS) {
            return nullptr;
        }

        LucGemObject* obj = &gem_slots_[handle - 1];
        if (obj->size == 0 && !obj->is_userptr) {
            return nullptr; // slot empty
        }
        return obj;
    }

    bool IntelGpuDevice::vm_bind(const lucifer_vm_bind& args) {
        IntelPpgtt* vm = lookup_vm(args.vm_id);
        if (!vm) {
            Log::log_dbc("intel-gpu: VM_BIND failed (bad vm_id)");
            return false;
        }

        if (args.op == LUCIFER_VM_BIND_OP_UNMAP) {
            // Simple first pass: point the range at the scratch page instead
            // of tearing down PT/PD/PDPT levels. Matches the scratch chain
            // IntelPpgtt already maintains for unmapped ranges elsewhere.
            return vm->insert_range(make_gfx(args.addr), make_phys(vm->scratch_page_phys_addr_bytes()), args.range,
                                     PpgttCaching::NONE, false);
        }

        phys_addr_t phys_start;
        PpgttCaching caching;
        if (args.op == LUCIFER_VM_BIND_OP_MAP_USERPTR) {
            // Bring-up limitation: userptr ranges aren't pinned/translated to
            // physical pages yet (no get_user_pages()-equivalent wired up
            // here), so there is no phys backing to hand insert_range().
            // Fail loudly rather than binding garbage.
            Log::log_dbc("intel-gpu: VM_BIND MAP_USERPTR not yet implemented");
            return false;
        }

        LucGemObject* obj = lookup_gem(args.handle);
        if (!obj || obj->is_userptr || obj->purged) {
            Log::log_dbc("intel-gpu: VM_BIND failed (bad handle)");
            return false;
        }

        Log::log_dbc("exec: mapping BO gpu_addr=0x%llx phys=0x%llx offset=0x%llu", (args.addr), phys_raw(obj->phys_addr), args.obj_offset);

        phys_start = phys_add(obj->phys_addr, args.obj_offset);

        // NOTE: args.pat_index is *not* used for caching here. Gen9.5
        // never populates devinfo->pat in Mesa (that table is only
        // filled from GFX12_PAT_ENTRIES onward)
        caching = obj->cpu_caching == LUCIFER_GEM_CPU_CACHING_WC
                      ? PpgttCaching::NONE
                      : PpgttCaching::LLC;

        // Bring-up: everything bound today is writable (matches Mesa's BOs,
        // which are all CPU+GPU read/write). Read-only mappings would need a
        // flag threaded through lucifer_vm_bind first.
        constexpr bool writable = true;

        return vm->insert_range(make_gfx(args.addr), phys_start, args.range, caching, writable);
    }

    IntelEngine* IntelGpuDevice::engine_for_class(const u32 engine_class) const {
        switch (engine_class) {
            case LUCIFER_ENGINE_CLASS_RENDER:
                return rcs_;
            case LUCIFER_ENGINE_CLASS_COPY:
                return bcs_;
            default:
                return nullptr;
        }
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

    bool IntelGpuDevice::exec_submit(lucifer_exec& args) {
        IntelPpgtt* vm = lookup_vm(args.vm_id);
        if (!vm) {
            Log::log_dbc("intel-gpu: EXEC failed (bad vm_id)");
            return false;
        }

        IntelEngine* engine = engine_for_class(args.engine);
        if (!engine) {
            Log::log_dbc("intel-gpu: EXEC failed (bad or unregistered engine=%u)", args.engine);
            return false;
        }

        if (args.num_syncs != 0 && args.syncs == 0) {
            Log::log_dbc("intel-gpu: EXEC failed (num_syncs=%u but syncs=NULL)", args.num_syncs);
            return false;
        }

        const auto* syncs = reinterpret_cast<const lucifer_sync*>(args.syncs);

        // Validate every sync entry up front -- both WAIT and SIGNAL
        // handles must exist, and SIGNAL handles must currently be
        // fence-less, or we must fail without having submitted anything.
        // Mirrors the old single out_syncobj check, just over an array.
        for (u32 i = 0; i < args.num_syncs; ++i) {
            const u32 handle = syncs[i].handle;

            if (handle == 0 || handle > MAX_LUCIFER_SYNCOBJS || !syncobj_slots_[handle - 1].in_use) {
                Log::log_dbc("intel-gpu: EXEC failed (bad sync handle=%u at index %u)", handle, i);
                return false;
            }

            if ((syncs[i].flags & LUCIFER_SYNC_FLAG_SIGNAL) && syncobj_slots_[handle - 1].has_fence) {
                Log::log_dbc("intel-gpu: EXEC failed (signal handle=%u already has a fence, reset it first)",
                             handle);
                return false;
            }
        }

        // Wait entries block dispatch of this batch -- wait-all semantics,
        // no timeout (submission-time ordering, not a userspace-visible
        // wait with its own deadline). Matches how Mesa/iris only ever
        // passes already-outstanding fences from prior submissions here.
        for (u32 i = 0; i < args.num_syncs; ++i) {
            if (syncs[i].flags & LUCIFER_SYNC_FLAG_SIGNAL) {
                continue;
            }

            const u32 handle = syncs[i].handle;
            if (syncobj_wait(&handle, 1, DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL, -1, nullptr) != 0) {
                Log::log_dbc("intel-gpu: EXEC failed (wait on sync handle=%u)", handle);
                return false;
            }
        }

        Log::log_dbc("exec: userspace batch_addr=0x%llx (from ioctl)", args.batch_addr);

        vm->dump_batch_buffer(make_gfx(args.batch_addr), args.batch_len);

        u32 seqno = 0;
        if (!engine->dispatch_batch(make_gfx(args.batch_addr), args.batch_len, &seqno, vm)) {
            Log::log_dbc("intel-gpu: EXEC failed (dispatch_batch)");
            return false;
        }

        args.out_seqno = seqno;

        // Can't fail from here on -- every SIGNAL handle was already
        // validated above, and no one else can have touched them between
        // the check and here (single-threaded ioctl dispatch).
        for (u32 i = 0; i < args.num_syncs; ++i) {
            if (syncs[i].flags & LUCIFER_SYNC_FLAG_SIGNAL) {
                syncobj_bind_fence(syncs[i].handle, args.engine, seqno);
            }
        }

        return true;
    }

    u32 IntelGpuDevice::syncobj_create(const drm_syncobj_create& args) {
        for (usize i = 0; i < MAX_LUCIFER_SYNCOBJS; ++i) {
            if (syncobj_slots_[i].in_use) {
                continue;
            }

            syncobj_slots_[i] = LucSyncObj{
                .in_use = true,
                .has_fence = false,
                .pre_signaled = (args.flags & DRM_SYNCOBJ_CREATE_SIGNALED) != 0,
            };

            return static_cast<u32>(i) + 1;
        }

        Log::log_dbc("intel-gpu: SYNCOBJ_CREATE failed (no free syncobj slots)");
        return 0;
    }

    bool IntelGpuDevice::syncobj_destroy(const u32 handle) {
        if (handle == 0 || handle > MAX_LUCIFER_SYNCOBJS) {
            return false;
        }

        LucSyncObj* obj = &syncobj_slots_[handle - 1];
        if (!obj->in_use) {
            return false;
        }

        *obj = LucSyncObj{};
        return true;
    }

    bool IntelGpuDevice::syncobj_bind_fence(const u32 handle, const u32 engine, const u64 target_seqno) {
        if (handle == 0 || handle > MAX_LUCIFER_SYNCOBJS) {
            return false;
        }

        LucSyncObj* obj = &syncobj_slots_[handle - 1];
        if (!obj->in_use || obj->has_fence) {
            return false;
        }

        obj->has_fence = true;
        obj->pre_signaled = false;
        obj->engine = engine;
        obj->target_seqno = target_seqno;
        return true;
    }

    /// True if `obj`'s current state is already signaled without needing to
    /// touch hardware: either force-signaled (SYNCOBJ_SIGNAL / just-created
    /// with DRM_SYNCOBJ_CREATE_SIGNALED), or its bound fence's seqno has
    /// already retired on its engine.
    bool IntelGpuDevice::syncobj_is_signaled_now(const IntelGpuDevice::LucSyncObj& obj,
                                        IntelEngine* engine) {
        if (!obj.has_fence) {
            return obj.pre_signaled;
        }
        return engine && *engine->seqno_ptr_for_read() >= static_cast<u32>(obj.target_seqno);
    }

    int IntelGpuDevice::syncobj_wait(const u32* handles, const u32 count_handles, const u32 flags,
                                     const i64 timeout_ns, u32* out_first_signaled) {
        if (!handles || count_handles == 0) {
            return -1;
        }

        for (u32 i = 0; i < count_handles; ++i) {
            if (handles[i] == 0 || handles[i] > MAX_LUCIFER_SYNCOBJS || !syncobj_slots_[handles[i] - 1].in_use) {
                Log::log_dbc("intel-gpu: SYNCOBJ_WAIT failed (bad handle=%u)", handles[i]);
                return -1;
            }
        }

        const bool wait_all = (flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL) != 0;

        // Bring-up note: no interrupt-driven multi-wait queue across
        // engines yet, so this polls at a fixed interval instead of
        // parking on engine_waiters_ the way single-handle
        // seqno_wait_blocking() does. Fine for bring-up; TODO(lucifer):
        // fold this into IntelEngine::seqno_wait_blocking()-style blocking
        // waits once cross-engine wait infra exists.
        const i64 poll_interval_us = 500;
        i64 waited_ns = 0;

        while (true) {
            u32 signaled_count = 0;

            for (u32 i = 0; i < count_handles; ++i) {
                const LucSyncObj& obj = syncobj_slots_[handles[i] - 1];
                IntelEngine* engine = obj.has_fence ? engine_for_class(obj.engine) : nullptr;

                if (obj.has_fence && engine && engine->is_banned()) {
                    Log::log_dbc("intel-gpu: SYNCOBJ_WAIT failed (handle=%u bound to banned engine=%u)",
                                 handles[i], obj.engine);
                    return -EIO;
                }

                if (syncobj_is_signaled_now(obj, engine)) {
                    signaled_count++;
                    if (!wait_all && out_first_signaled) {
                        *out_first_signaled = i;
                    }
                    if (!wait_all) {
                        return 0;
                    }
                }
            }

            if (wait_all && signaled_count == count_handles) {
                return 0;
            }

            if (timeout_ns >= 0 && waited_ns >= timeout_ns) {
                return -ETIME;
            }

            kernel::time::sleep_us(poll_interval_us);
            waited_ns += poll_interval_us * 1000;
        }
    }

    bool IntelGpuDevice::syncobj_reset(const u32* handles, const u32 count_handles) {
        if (!handles) {
            return false;
        }

        for (u32 i = 0; i < count_handles; ++i) {
            if (handles[i] == 0 || handles[i] > MAX_LUCIFER_SYNCOBJS || !syncobj_slots_[handles[i] - 1].in_use) {
                return false;
            }
        }

        for (u32 i = 0; i < count_handles; ++i) {
            LucSyncObj& obj = syncobj_slots_[handles[i] - 1];
            obj.has_fence = false;
            obj.pre_signaled = false;
            obj.engine = 0;
            obj.target_seqno = 0;
        }
        return true;
    }

    bool IntelGpuDevice::syncobj_signal(const u32* handles, const u32 count_handles) {
        if (!handles) {
            return false;
        }

        for (u32 i = 0; i < count_handles; ++i) {
            if (handles[i] == 0 || handles[i] > MAX_LUCIFER_SYNCOBJS || !syncobj_slots_[handles[i] - 1].in_use) {
                return false;
            }
        }

        for (u32 i = 0; i < count_handles; ++i) {
            LucSyncObj& obj = syncobj_slots_[handles[i] - 1];
            obj.has_fence = false;
            obj.pre_signaled = true;
        }
        return true;
    }

    bool IntelGpuDevice::query_gem_object(const u32 handle, GemObjectInfo* out) {
        if (!out) {
            return false;
        }

        const LucGemObject* obj = lookup_gem(handle);
        if (!obj || obj->is_userptr) {
            // Userptr objects have no kernel-owned phys backing (see
            // vm_bind()'s MAP_USERPTR path) — nothing for mmap() to map yet.
            return false;
        }

        *out = GemObjectInfo{.phys_addr = obj->phys_addr, .size = obj->size};
        return true;
    }

    bool IntelGpuDevice::gem_mmap_offset(const lucifer_gem_mmap_offset& args, u64* out_offset) {
        if (!out_offset) {
            return false;
        }

        const LucGemObject* obj = lookup_gem(args.handle);
        if (!obj || obj->is_userptr) {
            Log::log_dbc("intel-gpu: GEM_MMAP_OFFSET failed (bad handle)");
            return false;
        }

        // Handle -> offset is a direct page-number encoding (handle 1 lands
        // at page 1, etc.) rather than a separate lookup table: handles are
        // already small, dense, 1-based integers (see gem_create()'s slot
        // scheme), so this is already unique and trivially reversible via
        // gem_handle_from_mmap_offset() below. Revisit if GEM handles ever
        // stop being small dense integers.
        *out_offset = static_cast<u64>(args.handle) * PAGE_SIZE;
        return true;
    }

    u32 IntelGpuDevice::gem_handle_from_mmap_offset(const u64 offset) const {
        if (offset == 0 || offset % PAGE_SIZE != 0) {
            return 0;
        }

        const u64 handle = offset / PAGE_SIZE;
        if (handle == 0 || handle > MAX_LUCIFER_GEM_OBJECTS) {
            return 0;
        }

        return static_cast<u32>(handle);
    }

    kernel::vm::VmBackingObject* IntelGpuDevice::get_backing_object(CharFile*, const u64 offset) {
        const u32 handle = gem_handle_from_mmap_offset(offset);
        if (handle == 0) {
            Log::log_dbc("intel-gpu: mmap failed (offset does not decode to a GEM handle)");
            return nullptr;
        }

        GemObjectInfo info{};
        if (!query_gem_object(handle, &info)) {
            Log::log_dbc("intel-gpu: mmap failed (bad handle or userptr object)");
            return nullptr;
        }

        return new IntelGemBackingObject(this, handle);
    }

    int IntelGpuDevice::open(CharFile** out_cf) {
        if (!out_cf) {
            return -EINVAL;
        }

        auto* cf = new CharFile{};
        cf->driver_private = this;

        *out_cf = cf;
        return 0;
    }

    int IntelGpuDevice::release(CharFile* cf) {
        delete cf;
        return 0;
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

        if (request == LUCIFER_IOCTL_VM_CREATE) {
            auto* create = static_cast<lucifer_vm_create*>(arg);
            if (!create) {
                return -1;
            }

            const u32 vm_id = create_vm();
            if (vm_id == 0) {
                return -1;
            }

            create->vm_id = vm_id;
            return 0;
        }

        if (request == LUCIFER_IOCTL_VM_DESTROY) {
            const auto* destroy = static_cast<lucifer_vm_destroy*>(arg);
            if (!destroy) {
                return -1;
            }

            return destroy_vm(destroy->vm_id) ? 0 : -1;
        }

        if (request == LUCIFER_IOCTL_GEM_CREATE) {
            auto* create = static_cast<lucifer_gem_create*>(arg);
            if (!create) {
                return -1;
            }

            const u32 handle = gem_create(*create);
            if (handle == 0) {
                return -1;
            }

            create->handle = handle;
            return 0;
        }

        if (request == LUCIFER_IOCTL_GEM_USERPTR) {
            auto* userptr = static_cast<lucifer_gem_userptr*>(arg);
            if (!userptr) {
                return -1;
            }

            const u32 handle = gem_create_userptr(*userptr);
            if (handle == 0) {
                return -1;
            }

            userptr->handle = handle;
            return 0;
        }

        if (request == LUCIFER_IOCTL_GEM_CLOSE) {
            const auto* close = static_cast<lucifer_gem_close*>(arg);
            if (!close) {
                return -1;
            }

            return gem_close(close->handle) ? 0 : -1;
        }

        if (request == LUCIFER_IOCTL_GEM_MADVISE) {
            auto* madvise = static_cast<lucifer_gem_madvise*>(arg);
            if (!madvise) {
                return -1;
            }

            bool retained = false;
            if (!gem_madvise(*madvise, &retained)) {
                return -1;
            }

            madvise->retained = retained ? 1 : 0;
            return 0;
        }

        if (request == LUCIFER_IOCTL_VM_BIND) {
            const auto* bind = static_cast<lucifer_vm_bind*>(arg);
            if (!bind) {
                return -1;
            }

            return vm_bind(*bind) ? 0 : -1;
        }

        if (request == LUCIFER_IOCTL_GEM_MMAP_OFFSET) {
            auto* mmap_offset = static_cast<lucifer_gem_mmap_offset*>(arg);
            if (!mmap_offset) {
                return -1;
            }

            return gem_mmap_offset(*mmap_offset, &mmap_offset->offset) ? 0 : -1;
        }

        if (request == LUCIFER_IOCTL_EXEC) {
            auto* exec = static_cast<lucifer_exec*>(arg);
            if (!exec) {
                return -1;
            }

            return exec_submit(*exec) ? 0 : -1;
        }

        if (request == DRM_IOCTL_SYNCOBJ_CREATE) {
            auto* create = static_cast<drm_syncobj_create*>(arg);
            if (!create) {
                return -1;
            }

            const u32 handle = syncobj_create(*create);
            if (handle == 0) {
                return -1;
            }

            create->handle = handle;
            return 0;
        }

        if (request == DRM_IOCTL_SYNCOBJ_DESTROY) {
            const auto* destroy = static_cast<drm_syncobj_destroy*>(arg);
            if (!destroy) {
                return -1;
            }

            return syncobj_destroy(destroy->handle) ? 0 : -1;
        }

        if (request == DRM_IOCTL_SYNCOBJ_WAIT) {
            auto* wait = static_cast<drm_syncobj_wait*>(arg);
            if (!wait) {
                return -1;
            }

            // wait->handles is a userspace u64 holding a pointer to a
            // u32[count_handles] array, per generic DRM syncobj ABI --
            // mirrors how gem_syncobj_create()-style callers in Mesa build
            // this struct today.
            const auto* handles = reinterpret_cast<const u32*>(wait->handles);

            return syncobj_wait(handles, wait->count_handles, wait->flags,
                                wait->timeout_nsec, &wait->first_signaled);
        }

        if (request == DRM_IOCTL_SYNCOBJ_RESET) {
            const auto* reset = static_cast<drm_syncobj_array*>(arg);
            if (!reset) {
                return -1;
            }

            const auto* handles = reinterpret_cast<const u32*>(reset->handles);
            return syncobj_reset(handles, reset->count_handles) ? 0 : -1;
        }

        if (request == DRM_IOCTL_SYNCOBJ_SIGNAL) {
            const auto* signal = static_cast<drm_syncobj_array*>(arg);
            if (!signal) {
                return -1;
            }

            const auto* handles = reinterpret_cast<const u32*>(signal->handles);
            return syncobj_signal(handles, signal->count_handles) ? 0 : -1;
        }

        /* TODO(lucifer): DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD/FD_TO_HANDLE and
         * timeline-point variants (drm_syncobj_timeline_wait, TRANSFER,
         * eventfd) are declared in drm.h but not dispatched here yet */

        if (request != LUCIFER_IOCTL_QUERY) {
            Log::debug("unknown DRM lucifer ioctl. request=%x", request);
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

                out->gtt_size = PPGTT_VA_SPACE_SIZE;
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
} // namespace gpu::intel::core
