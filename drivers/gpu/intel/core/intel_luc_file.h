// intel_luc_file.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 19.09.26.
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

#ifndef VESPERAOS_INTEL_LUC_FILE_H
#define VESPERAOS_INTEL_LUC_FILE_H

#include <vespera/mm/addr.h>
#include <vespera/mm/vm_backing.h>
#include <vespera/sync/spinlock.h>
#include <vespera/types.h>

#include "intel_engine.h"
#include "intel_ppgtt.h"
#include "slot_table.h"
#include "uapi/vespera/dev/drm.h"
#include "uapi/vespera/dev/lucifer_drm.h"

namespace gpu::intel::core {
    class IntelGpuDevice;
    class IntelEngine;

    /// Refcounted bring-up bookkeeping for one GEM object. Heap-allocated
    /// (see LucFile::gem_slots_) so a reference can outlive the handle
    /// slot that created it -- e.g. an IntelGemBackingObject from an
    /// in-progress mmap() outliving a concurrent GEM_CLOSE on the same
    /// handle.
    ///
    /// Deliberately thin for now -- just enough for gem_create/
    /// gem_create_userptr/gem_close to round-trip a handle plus a real
    /// refcount. No GGTT/PPGTT binding lives here; that's VM_BIND's job
    /// (see LucFile::vm_bind()).
    ///
    /// Still a struct, not a class: unlike LucFile below, GemObject has no
    /// invariant beyond its own refcount (guarded by inc_ref()/dec_ref()
    /// themselves, not by hiding the other fields) -- every other field is
    /// legitimately just data callers read directly (size, phys_addr,
    /// is_userptr, ...), so there's nothing an access specifier would be
    /// protecting.
    struct GemObject {
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

        /// Refs: the handle slot itself (dropped by gem_close()/LucFile
        /// teardown) plus one per live IntelGemBackingObject
        /// (add_mapping()/remove_mapping()). Pages are freed -- and the
        /// object deleted -- only when this reaches zero, so a handle
        /// closed while still mmap()'d stays valid for the existing
        /// mapping until that mapping goes away too.
        u32 refcount = 0;

        void inc_ref() {
            refcount++;
        }

        /// Returns true if this was the last reference (pages already
        /// freed and *this already destroyed by the time this returns --
        /// never touch the object again after a call that returns true).
        [[nodiscard]] bool dec_ref();
    };
    class LucFile {
    public:
        /// `file_id` must be unique among every LucFile currently open on
        /// `device` -- see context_for_engine() for why (it derives each
        /// per-engine LRC's sw_context_id from it).
        explicit LucFile(IntelGpuDevice& device, u32 file_id) : device_(device), file_id_(file_id) {
        }

        ~LucFile();

        LucFile(const LucFile&) = delete;
        LucFile& operator=(const LucFile&) = delete;

        int ioctl(u32 request, void* arg);

        [[nodiscard]] kernel::vm::VmBackingObject* get_backing_object(u64 offset) const;

        /// Takes a ref on the GEM object behind `handle` and returns it,
        /// for callers (IntelGemBackingObject) that need to outlive the
        /// ioctl call that looked the handle up. nullptr on a bad handle.
        [[nodiscard]] GemObject* gem_get_ref(u32 handle) const;

    private:
        static constexpr usize MAX_LUCIFER_VMS = 32;
        SlotTable<IntelPpgtt*> vm_slots_;

        [[nodiscard]] u32 create_vm();
        bool destroy_vm(u32 vm_id);
        [[nodiscard]] IntelPpgtt* lookup_vm(u32 vm_id) const;

        static constexpr usize MAX_LUCIFER_GEM_OBJECTS = 4096;
        SlotTable<GemObject*> gem_slots_;

        [[nodiscard]] u32 gem_create(const lucifer_gem_create& args);
        [[nodiscard]] u32 gem_create_userptr(const lucifer_gem_userptr& args);
        bool gem_close(u32 handle);
        [[nodiscard]] GemObject* lookup_gem(u32 handle) const;

        [[nodiscard]] bool gem_madvise(const lucifer_gem_madvise& args, bool* out_retained) const;

        [[nodiscard]] bool gem_mmap_offset(const lucifer_gem_mmap_offset& args, u64* out_offset);
        [[nodiscard]] u32 gem_handle_from_mmap_offset(u64 offset) const;

        struct GemObjectInfo {
            phys_addr_t phys_addr;
            u64 size;
        };
        [[nodiscard]] bool query_gem_object(u32 handle, GemObjectInfo* out) const;

        bool vm_bind(const lucifer_vm_bind& args) const;

        [[nodiscard]] bool exec_submit(lucifer_exec& args);

        static constexpr usize LUCIFER_NUM_ENGINE_CLASSES = 2; // RENDER, COPY -- mirrors IntelGpuDevice's
        EngineContext engine_contexts_[LUCIFER_NUM_ENGINE_CLASSES];
        bool engine_contexts_initialized_[LUCIFER_NUM_ENGINE_CLASSES] = {};

        /// Returns this file's EngineContext for `engine`, allocating and
        /// initializing its LRC on first call
        [[nodiscard]] EngineContext* context_for_engine(IntelEngine* engine, u32 engine_class);
        [[nodiscard]] EngineContext* context_for_engine_locked(IntelEngine* engine, u32 engine_class);

        struct LucSyncObj {
            bool in_use = false;
            bool has_fence = false;
            bool pre_signaled = false;
            u32 engine = 0;
            u64 target_seqno = 0;
        };

        static constexpr usize MAX_LUCIFER_SYNCOBJS = 4096;
        SlotTable<LucSyncObj> syncobj_slots_;

        [[nodiscard]] u32 syncobj_create(const drm_syncobj_create& args);
        bool syncobj_destroy(u32 handle);
        [[nodiscard]] bool syncobj_bind_fence(u32 handle, u32 engine, u64 target_seqno);
        bool syncobj_is_signaled_now(const LucSyncObj& obj, IntelEngine* engine);
        [[nodiscard]] int syncobj_wait(const u32* handles, u32 count_handles, u32 flags,
                                       i64 timeout_ns, u32* out_first_signaled);
        bool syncobj_reset(const u32* handles, u32 count_handles);
        bool syncobj_signal(const u32* handles, u32 count_handles);

        IntelGpuDevice& device_;

        u32 file_id_{0u};

        mutable Spinlock handle_lock_{"luc_file_handles"};
    };
} // namespace gpu::intel::core

#endif  // VESPERAOS_INTEL_LUC_FILE_H
