// intel_gpu_device.h
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
#ifndef VESPERAOS_INTEL_GPU_DEVICE_H
#define VESPERAOS_INTEL_GPU_DEVICE_H

#include <pci/pci_id.h>
#include <vespera/interrupts.h>
#include <vespera/mm/addr.h>
#include <vespera/mm/vm_backing.h>
#include <vespera_errno.h>

#include "ggtt_allocator.h"
#include <gpu/intel/regs/pci_config_regs.h>

#include "intel_ppgtt.h"
#include "vespera/devices/char_device.h"
#include "vespera/devices/device_info.h"
#include "vespera/devices/kernel_device.h"

#include "uapi/vespera/dev/lucifer_drm.h"
#include "uapi/vespera/dev/drm.h"
#include <vespera/sync/wait_queue.h>

namespace pci {
    struct pci_device;
}

namespace gpu::intel::bcs {
    class IntelBcs;
}

namespace gpu::intel::rcs {
    class IntelRcs;
}

namespace gpu::intel::core {
    class IntelEngine;

    constexpr u64 GTTMMADR_ADDR_MASK = ~0xFULL;
    constexpr usize BAR0_SIZE = 16ull * 1024 * 1024;

    /// Number of GT0-registered engines this device can dispatch interrupts
    /// to. Fixed-size array rather than a dynamic container — the engine set
    /// is known at compile time (RCS, BCS today; VCS/VECS later) and this
    /// path runs from interrupt context, where allocation is off-limits.
    constexpr usize MAX_GT_IRQ_ENGINES = 4;

    /**
     * @brief Descriptor for one Gen9 ForceWake power domain.
     *
     * Gen9 splits ForceWake per engine — Render, Blitter, and Media each have
     * their own request+ack MMIO register pair (see FORCEWAKE_RENDER in
     * intel_rcs.h, FORCEWAKE_BLITTER in intel_bcs.h). IntelGpuDevice itself
     * has no opinion on which domains exist or what their offsets are — it
     * just drives whichever descriptor the calling IntelEngine hands it.
     */
    struct ForceWakeDomain {
        u32 request_reg;  ///< MMIO offset of the request register
        u32 ack_reg;      ///< MMIO offset of the ack register
        u32 enable_value; ///< value to write to the request register
        u32 ack_bit;      ///< bit to poll for in the ack register
        u32 timeout_us;
    };

    /**
     * @brief Everything shared by every command-streamer engine on one
     *        physical Intel iGPU: BAR0 mapping, PCI config, and the single
     *        GGTT.
     *
     * One instance per physical GPU, constructed and init()'d by
     * IntelBltPciDriver before any engine (IntelBcs, IntelRcs) is
     * constructed — every engine's constructor takes a reference to this and
     * borrows its MMIO base / GGTT via IntelEngine's protected accessors.
     * Neither engine owns this device, and neither engine owns the other.
     *
     * GT0 interrupt ownership: GT0_ISR/IMR/IIR/IER (MMIO 0x44300) is a
     * SINGLE register group shared between RCS (bits [15:0]) and BCS (bits
     * [31:16]) — see gt_interrupt_regs.h. Because it's one physical register
     * two engines legitimately share, GT0 IMR/IER programming and the single
     * MSI/MSI-X handler both live here rather than in either engine class:
     * an engine-owned handler would either need to know about its sibling
     * engine's bits (breaking the "neither engine owns the other" invariant
     * above) or risk clobbering them via a naive full-register write.
     */
    class IntelGpuDevice final : public IDeviceInfo, public IRenderDriver, public CharDevice {
    public:
        /// Sentinel returned by allocate_irq_vector() on failure.
        static constexpr u8 INVALID_VECTOR = 0xFF;

        explicit IntelGpuDevice(const pci::pci_device& igpu_dev);

        IntelGpuDevice(const IntelGpuDevice&) = delete;
        IntelGpuDevice& operator=(const IntelGpuDevice&) = delete;

        /// Sets up the GGTT (host bridge GGC + GMADR/MSAC discovery). BAR0 is
        /// already mapped by the constructor. Must succeed before any
        /// engine's init_device() is called.
        [[nodiscard]] bool init();

        [[nodiscard]] volatile u8* mmio_base() const {
            return mmio_base_;
        }

        [[nodiscard]] GgttAllocator& ggtt() {
            return ggtt_alloc_;
        }

        [[nodiscard]] volatile INTEL_IGP_PCI_CONFIG* pci_cfg() const {
            return igp_cfg_;
        }

        [[nodiscard]] const pci::pci_id& pci_id() const {
            return pci_id_;
        }

        [[nodiscard]] KernelDevice* get_kd() const {
            return kd_;
        }

        bool get_vendor(char* out, usize len) override;
        bool get_model(char* out, usize len) override;

        bool fill_rect(u32 px, u32 py, u32 w, u32 h, u32 colour) override;
        bool blit_region(
            const u32* pixels, u32 src_stride, u32 src_x, u32 src_y, u32 w, u32 h, u32 dst_x, u32 dst_y
        ) override;

        void present() override;

        [[nodiscard]] u32 screen_width_px() const override;

        [[nodiscard]] u32 screen_height_px() const override;

        [[nodiscard]] u32 bytes_per_scanline() const override;

        void add_bcs(bcs::IntelBcs* engine) {
            bcs_ = engine;
        }

        void add_rcs(rcs::IntelRcs* engine) {
            rcs_ = engine;
        }

        int open(CharFile** out_cf) override;
        int release(CharFile* cf) override;


        struct GemObjectInfo {
            phys_addr_t phys_addr;
            u64 size;
        };
        [[nodiscard]] bool query_gem_object(u32 handle, GemObjectInfo* out);

        isize read(CharFile* cf, void* buffer, usize count, usize offset) override { return -ENOTTY; };
        isize write(CharFile* cf, const void* buffer, usize count) override { return -ENOTTY; };

        int ioctl(CharFile*, u32, void*) override;

        /// Decodes `offset` back into a GEM handle via
        /// gem_handle_from_mmap_offset() and hands back an
        /// IntelGemBackingObject for it, so kernel::vm::mmap() can map a
        /// GEM object without linking against anything GEM- or GPU-
        /// specific. Returns nullptr for userptr handles (no kernel-owned
        /// phys backing — see query_gem_object()) or an offset that
        /// doesn't decode to a live handle.
        [[nodiscard]] kernel::vm::VmBackingObject* get_backing_object(CharFile* cf, u64 offset) override;

        /// Drives one ForceWake domain: write request, poll ack, timeout.
        /// Stateless with respect to which domain is being woken — the
        /// caller supplies its own descriptor (see IntelEngine::engine_force_wake_enable()).
        [[nodiscard]] bool force_wake_enable(const ForceWakeDomain& domain) const;

        /// Lazily allocates one shared MSI/MSI-X vector for this device and
        /// wires it up via try_enable_msi_or_msix(). Exposed publicly for
        /// non-GT0 interrupt sources (e.g. DE pipe / display); GT0-routed
        /// engines should go through register_engine_for_irq() instead,
        /// which calls this internally.
        [[nodiscard]] u8 allocate_irq_vector(irq_handler_t handler, void* ctx);

        /// Registers `engine` to receive on_gt_user_interrupt() callbacks
        /// whenever its gt_user_irq_bit() fires in GT0_IIR. Lazily installs
        /// the shared device-level MSI vector and GT0 dispatcher on first
        /// call (from any engine) — safe to call from every engine's
        /// init_device(), regardless of order. Also unmasks/enables this
        /// engine's own bit in GT0_IMR/IER via read-modify-write, leaving
        /// any other already-registered engine's bit untouched, and ensures
        /// MASTER_INT_CTL's global enable + GT routing bits are set without
        /// disturbing any other subsystem's pending/enable bits (Display,
        /// Audio, PCU, ...) that may already be live.
        [[nodiscard]] bool register_engine_for_irq(IntelEngine* engine);

        /// Registers a callback for DE Pipe A's vblank + plane-1-flip-done
        /// interrupts (used by IntelBcs for present()/page-flip completion).
        /// Lives here for the same reason GT0 does: MASTER_INT_CTL is a
        /// single device-wide register, so whoever owns display flip timing
        /// must not be the same code path that owns MASTER_INT_CTL's global
        /// enable bit in isolation. `ctx` is passed back on every callback
        /// invocation unchanged.
        using DePipeAHandler = void (*)(void* ctx, bool vblank, bool plane1_flip_done);
        [[nodiscard]] bool register_de_pipe_a_handler(DePipeAHandler handler, void* ctx);

        /// Arms (unmasks + enables) DE Pipe A's vblank interrupt for exactly
        /// one firing — mirrors the one-shot mask/disable IntelBcs already
        /// does today when a flip completes. Call this right before issuing
        /// a flip that needs a vblank wakeup.
        void de_pipe_a_arm_vblank_oneshot() const;

        /// Re-masks + disables DE Pipe A's vblank interrupt. Call this from
        /// the DE Pipe A callback once the flip being waited on completes —
        /// undoes de_pipe_a_arm_vblank_oneshot() so vblank stops firing
        /// until the next flip explicitly re-arms it.
        void de_pipe_a_disarm_vblank() const;

    private:
        /// The single MSI/MSI-X handler installed for this device's GT0 +
        /// DE Pipe A interrupts. Reads MASTER_INT_CTL once to see which
        /// subsystem is pending, dispatches to the GT0 engine registry
        /// and/or the DE Pipe A callback as appropriate, then re-arms
        /// MASTER_INT_CTL's global enable bit without touching any other
        /// subsystem's pending bits (those are RO and self-clear anyway).
        static Irqreturn device_irq_handler(IntelGpuDevice* self);

        /// Sets MASTER_INT_CTL.master_enable=1 via read-modify-write,
        /// preserving whatever else is currently there. Safe to call
        /// multiple times / from multiple subsystems — never zeroes the
        /// register first the way the old per-engine init paths did.
        void master_int_ctl_enable() const;

        struct FuseTopology {
            u32 slice_count;
            u32 subslice_count;
            u32 eu_count;
        };

        [[nodiscard]] FuseTopology query_fuse_topology() const;

        static constexpr usize MAX_LUCIFER_VMS = 64;
        IntelPpgtt* vm_slots_[MAX_LUCIFER_VMS] = {};

        [[nodiscard]] u32 create_vm();
        bool destroy_vm(u32 vm_id);
        [[nodiscard]] IntelPpgtt* lookup_vm(u32 vm_id) const;

        /// Minimal bring-up bookkeeping for one GEM object. Deliberately
        /// thin for now — just enough for gem_create/gem_create_userptr/
        /// gem_close to round-trip a handle. No backing allocation lives
        /// here yet (no phys pages, no GGTT/PPGTT binding); that arrives
        /// with VM_BIND. Mirrors the vm_slots_ handle scheme below rather
        /// than introducing a second allocation strategy.
        struct LucGemObject {
            u64 size = 0;         ///< requested size in bytes (0 for userptr)
            u32 placement = 0;    ///< memory region bitmask, from GEM_CREATE
            u32 flags = 0;         ///< enum lucifer_gem_create_flags
            u32 cpu_caching = 0;   ///< enum lucifer_gem_cpu_caching
            bool is_userptr = false;
            u64 userptr = 0;       ///< user VA, only valid when is_userptr

            /// Physical backing, allocated up front in gem_create()
            phys_addr_t phys_addr = phys_addr_t{};

            /// GEM_MADVISE state. false (WILLNEED, the default) means the
            /// backing above is live. true (DONTNEED) means gem_madvise()
            /// has already returned phys_addr's pages to the allocator
            bool purged = false;
        };

        static constexpr usize MAX_LUCIFER_GEM_OBJECTS = 4096;
        LucGemObject gem_slots_[MAX_LUCIFER_GEM_OBJECTS] = {};

        [[nodiscard]] u32 gem_create(const struct lucifer_gem_create& args);
        [[nodiscard]] u32 gem_create_userptr(const struct lucifer_gem_userptr& args);
        bool gem_close(u32 handle);
        [[nodiscard]] LucGemObject* lookup_gem(u32 handle);

        /// Backs LUCIFER_IOCTL_GEM_MADVISE. WILLNEED on an already-live
        /// object is a no-op that reports retained. DONTNEED frees
        /// phys_addr's pages via free_pages_phys() and marks the object
        /// purged; a later WILLNEED on a purged object reports not
        /// retained (backing store lost) rather than reallocating it --
        /// matching lucifer_bo_madvise()'s userspace contract, which
        /// expects the caller to recreate the BO in that case. Returns
        /// false only for a bad/userptr handle; out_retained is only
        /// meaningful when this returns true.
        [[nodiscard]] bool gem_madvise(const struct lucifer_gem_madvise& args, bool* out_retained);

        /// Encodes a GEM handle into a page-aligned "fake" mmap offset for
        /// LUCIFER_IOCTL_GEM_MMAP_OFFSET
        [[nodiscard]] bool gem_mmap_offset(const struct lucifer_gem_mmap_offset& args, u64* out_offset);

        /// Inverse of gem_mmap_offset(): recovers the GEM handle encoded in
        /// an mmap() offset. Returns 0 (invalid handle) if `offset` isn't
        /// page-aligned or doesn't decode to a live GEM object.
        [[nodiscard]] u32 gem_handle_from_mmap_offset(u64 offset) const;

        /// Binds/unbinds a GEM object (or userptr range) into the PPGTT
        /// identified by vm_id.
        bool vm_bind(const struct lucifer_vm_bind& args);

        /// Backs DRM_IOCTL_SYNCOBJ_WAIT (see syncobj_wait() below)
        static constexpr usize LUCIFER_NUM_ENGINE_CLASSES = 2; // RENDER, COPY
        WaitQueue engine_waiters_[LUCIFER_NUM_ENGINE_CLASSES];

        [[nodiscard]] IntelEngine* engine_for_class(u32 engine_class) const;

        void engine_signal_seqno(u32 engine_class);

        /// Submits one batch to the given engine via
        /// IntelEngine::dispatch_batch(). Returns the assigned seqno via
        /// args.out_seqno, or false on failure (bad vm_id/engine/handle,
        /// bad batch, or a bad entry in args.syncs).
        ///
        /// args.syncs points to an array of args.num_syncs
        /// struct lucifer_sync entries (userspace pointer, mirrors how
        /// DRM_IOCTL_SYNCOBJ_WAIT/RESET/SIGNAL already pass their handle
        /// arrays below). Entries WITHOUT LUCIFER_SYNC_FLAG_SIGNAL are WAIT
        /// entries: all of them are waited on (blocking, wait-all) before
        /// the batch is dispatched. Entries WITH the flag set are SIGNAL
        /// entries: once dispatch succeeds, each has this submission's
        /// (engine, seqno) fence bound onto it, same as the old single
        /// out_syncobj field used to.
        ///
        /// Every SIGNAL handle is validated (exists, currently fence-less)
        /// *before* the batch is dispatched -- never submit and then fail
        /// to attach a fence. WAIT handles just need to exist; being
        /// currently fence-less is fine (an unsignaled wait handle blocks
        /// until SIGNALed/timeout, matching normal DRM syncobj semantics).
        [[nodiscard]] bool exec_submit(lucifer_exec& args);

        /// One DRM syncobj slot. A syncobj is a handle to a fence, not a
        /// fence itself
        ///
        /// Bring-up scope: binary only (signaled or not) -- no timeline
        /// points, no fd import/export.
        struct LucSyncObj {
            bool in_use = false;      ///< handle is allocated
            bool has_fence = false;   ///< a fence is currently bound (false right after CREATE, or after RESET)
            bool pre_signaled = false; ///< set by DRM_SYNCOBJ_CREATE_SIGNALED when has_fence is also false
            u32 engine = 0;            ///< enum lucifer_engine_class, valid iff has_fence
            u64 target_seqno = 0;      ///< valid iff has_fence
        };

        static constexpr usize MAX_LUCIFER_SYNCOBJS = 4096;
        LucSyncObj syncobj_slots_[MAX_LUCIFER_SYNCOBJS] = {};

        [[nodiscard]] u32 syncobj_create(const struct drm_syncobj_create& args);
        bool syncobj_destroy(u32 handle);

        /// Binds this submission's (engine, seqno) fence onto an existing,
        /// currently fence-less syncobj handle. Called by exec_submit() for
        /// each SIGNAL entry in lucifer_exec::syncs, after dispatch. Fails
        /// (returns false) if the handle doesn't exist or already holds a
        /// live fence -- callers must RESET a reused handle first. By the
        /// time exec_submit() calls this it has already been validated, so
        /// in practice this can't fail from that call site.
        [[nodiscard]] bool syncobj_bind_fence(u32 handle, u32 engine, u64 target_seqno);
        bool syncobj_is_signaled_now(const LucSyncObj& obj, IntelEngine* engine);

        /// Backs DRM_IOCTL_SYNCOBJ_WAIT.
        [[nodiscard]] int syncobj_wait(const u32* handles, u32 count_handles, u32 flags,
                                       i64 timeout_ns, u32* out_first_signaled);

        /// Backs DRM_IOCTL_SYNCOBJ_RESET: clears the bound fence (if any)
        /// on each of the given handles, leaving them allocated but
        /// fence-less -- ready to be reused by a future exec_submit().
        bool syncobj_reset(const u32* handles, u32 count_handles);

        /// Backs DRM_IOCTL_SYNCOBJ_SIGNAL: force-signals each of the given
        /// handles immediately, independent of any engine seqno. Bring-up
        /// note: modeled as pre_signaled = true, has_fence = false, since
        /// there's no real fence object to mark signaled here -- a
        /// subsequent WAIT on the handle succeeds immediately, matching
        /// observable DRM semantics even though the underlying
        /// representation is simplified.
        bool syncobj_signal(const u32* handles, u32 count_handles);

        volatile INTEL_IGP_PCI_CONFIG* igp_cfg_;
        pci::pci_id pci_id_;
        volatile u8* mmio_base_ = nullptr;
        GgttAllocator ggtt_alloc_;
        u8 irq_vector_ = INVALID_VECTOR;

        IntelEngine* gt_irq_engines_[MAX_GT_IRQ_ENGINES] = {};
        usize gt_irq_engine_count_ = 0;

        DePipeAHandler de_pipe_a_handler_ = nullptr;
        void* de_pipe_a_handler_ctx_ = nullptr;

        bcs::IntelBcs* bcs_ = nullptr;
        rcs::IntelRcs* rcs_ = nullptr;

        KernelDevice* kd_ = nullptr;
    };
} // namespace blt

#endif  // VESPERAOS_INTEL_GPU_DEVICE_H
