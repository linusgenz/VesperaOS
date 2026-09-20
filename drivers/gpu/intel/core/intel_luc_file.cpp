// intel_luc_file.cpp
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

#include "intel_luc_file.h"

#include <klib/string.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/time.h>

#include "intel_engine.h"
#include "intel_gem_backing.h"
#include "intel_gpu_device.h"

namespace gpu::intel::core {
    bool GemObject::dec_ref() {
        refcount--;
        if (refcount != 0) {
            return false;
        }

        // purged objects already returned their pages in gem_madvise();
        // nothing left to free here beyond the object itself.
        if (!is_userptr && !purged) {
            const usize page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
            kernel::memory::free_pages_phys(phys_addr, page_count);
        }

        delete this;
        return true;
    }

    LucFile::~LucFile() {
        // 1. Syncobjs: no other table here depends on them, and nothing
        // downstream needs them intact. SlotTable<T>'s iterator visits
        // every slot including already-free ones -- harmless here since
        // resetting an already-default LucSyncObj{} to LucSyncObj{} again
        // is a no-op.
        for (auto& obj : syncobj_slots_) {
            obj = LucSyncObj{};
        }

        // 2. GEM refs: drop this file's own reference on every handle
        // still open. A concurrent IntelGemBackingObject from an
        // in-progress mmap() holds its own ref (see gem_get_ref()), so the
        // object survives until that mapping is torn down too -- this
        // only ever frees objects nothing else is using.
        for (auto*& obj : gem_slots_) {
            if (obj != nullptr) {
                obj->dec_ref();
                obj = nullptr;
            }
        }

        for (auto*& vm : vm_slots_) {
            if (vm != nullptr) {
                vm->destroy();
                delete vm;
                vm = nullptr;
            }
        }
    }

    u32 LucFile::create_vm() {
        if (vm_slots_.capacity() >= MAX_LUCIFER_VMS && !vm_slots_.has_free_slot()) {
            Log::log_dbc("intel-gpu: VM_CREATE failed (no free VM slots)");
            return 0;
        }

        auto* ppgtt = new IntelPpgtt(device_.ggtt());
        if (!ppgtt->init()) {
            delete ppgtt;
            Log::log_dbc("intel-gpu: VM_CREATE failed (IntelPpgtt::init)");
            return 0;
        }

        return vm_slots_.insert(ppgtt);
    }

    bool LucFile::destroy_vm(const u32 vm_id) {
        IntelPpgtt** slot = vm_slots_.get(vm_id);
        if (slot == nullptr || *slot == nullptr) {
            return false;
        }

        (*slot)->destroy();
        delete *slot;
        vm_slots_.remove(vm_id);
        return true;
    }

    IntelPpgtt* LucFile::lookup_vm(const u32 vm_id) const {
        // SlotTable<IntelPpgtt*>::get() const returns IntelPpgtt* const*
        // (pointer-to-const-pointer-to-non-const-IntelPpgtt): the slot
        // itself can't be reassigned through this handle, but the
        // IntelPpgtt it points at is still mutable, same as the raw
        // vm_slots_[vm_id - 1] read this replaces.
        IntelPpgtt* const* slot = vm_slots_.get(vm_id);
        return slot ? *slot : nullptr;
    }

    u32 LucFile::gem_create(const lucifer_gem_create& args) {
        if (gem_slots_.capacity() >= MAX_LUCIFER_GEM_OBJECTS && !gem_slots_.has_free_slot()) {
            Log::log_dbc("intel-gpu: GEM_CREATE failed (no free GEM slots)");
            return 0;
        }

        const usize page_count = (args.size + PAGE_SIZE - 1) / PAGE_SIZE;
        const phys_addr_t phys = kernel::memory::request_pages_phys(page_count);
        if (phys_null(phys)) {
            Log::log_dbc("intel-gpu: GEM_CREATE failed (request_pages_phys)");
            return 0;
        }

        memset(phys_to_virt(phys), 0, PAGE_SIZE * page_count);
        asm volatile("mfence" ::: "memory");

        auto* obj = new GemObject{
            .size        = args.size,
            .placement   = args.placement,
            .flags       = args.flags,
            .cpu_caching = args.cpu_caching,
            .is_userptr  = false,
            .userptr     = 0,
            .phys_addr   = phys,
        };
        obj->inc_ref(); // the handle slot's own reference

        return gem_slots_.insert(obj);
    }

    u32 LucFile::gem_create_userptr(const lucifer_gem_userptr& args) {
        if (gem_slots_.capacity() >= MAX_LUCIFER_GEM_OBJECTS && !gem_slots_.has_free_slot()) {
            Log::log_dbc("intel-gpu: GEM_USERPTR failed (no free GEM slots)");
            return 0;
        }

        auto* obj = new GemObject{
            .size        = args.size,
            .placement   = 0,
            .flags       = 0,
            .cpu_caching = LUCIFER_GEM_CPU_CACHING_WB,
            .is_userptr  = true,
            .userptr     = args.ptr,
        };
        obj->inc_ref(); // the handle slot's own reference

        return gem_slots_.insert(obj);
    }

    bool LucFile::gem_close(const u32 handle) {
        GemObject** slot = gem_slots_.get(handle);
        if (slot == nullptr || *slot == nullptr) {
            return false;
        }

        // Drop the handle slot's own reference. If an IntelGemBackingObject
        // from a still-open mmap() holds another ref, the object survives
        // this call (see gem_get_ref()) -- its pages are only freed once
        // every ref is gone, dec_ref() handles that.
        (*slot)->dec_ref();
        gem_slots_.remove(handle);
        return true;
    }

    bool LucFile::gem_madvise(const lucifer_gem_madvise& args, bool* out_retained) {
        GemObject* obj = lookup_gem(args.handle);
        if (!obj || obj->is_userptr) {
            Log::log_dbc("intel-gpu: GEM_MADVISE failed (bad handle)");
            return false;
        }

        if (args.state == LUCIFER_MADVICE_DONT_NEED) {
            if (!obj->purged) {
                // Bring-up assumption: caller has already VM_BIND UNMAP'd
                // this object on every VM it was bound to -- see the
                // `purged` field comment on GemObject. Not verified here
                // (no back-reference from GEM object to bindings).
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

    GemObject* LucFile::lookup_gem(const u32 handle) const {
        // SlotTable<GemObject*>::get() const returns GemObject* const*
        // (see lookup_vm()'s comment for why the const-level is correct
        // here): bounds-checks against the table's actual current size,
        // not the MAX_LUCIFER_GEM_OBJECTS cap -- a handle beyond the
        // cap can never have been issued in the first place, so get()'s
        // own out-of-range check already covers it.
        GemObject* const* slot = gem_slots_.get(handle);
        return slot ? *slot : nullptr;
    }

    GemObject* LucFile::gem_get_ref(const u32 handle) const {
        GemObject* obj = lookup_gem(handle);
        if (!obj) {
            return nullptr;
        }
        obj->inc_ref();
        return obj;
    }

    bool LucFile::vm_bind(const lucifer_vm_bind& args) {
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

        GemObject* obj = lookup_gem(args.handle);
        if (!obj || obj->is_userptr || obj->purged) {
            Log::log_dbc("intel-gpu: VM_BIND failed (bad handle)");
            return false;
        }

        Log::log_dbc("exec: mapping BO gpu_addr=0x%llx phys=0x%llx offset=0x%llu", (args.addr),
                     phys_raw(obj->phys_addr), args.obj_offset);

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

    bool LucFile::exec_submit(lucifer_exec& args) {
        IntelPpgtt* vm = lookup_vm(args.vm_id);
        if (!vm) {
            Log::log_dbc("intel-gpu: EXEC failed (bad vm_id)");
            return false;
        }

        IntelEngine* engine = device_.engine_for_class(args.engine);
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

            const LucSyncObj* obj = syncobj_slots_.get(handle);
            if (!obj || !obj->in_use) {
                Log::log_dbc("intel-gpu: EXEC failed (bad sync handle=%u at index %u)", handle, i);
                return false;
            }

            if ((syncs[i].flags & LUCIFER_SYNC_FLAG_SIGNAL) && obj->has_fence) {
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

        Log::log_dbc("returning to userspace from submit");
        return true;
    }

    u32 LucFile::syncobj_create(const drm_syncobj_create& args) {
        if (syncobj_slots_.capacity() >= MAX_LUCIFER_SYNCOBJS && !syncobj_slots_.has_free_slot()) {
            Log::log_dbc("intel-gpu: SYNCOBJ_CREATE failed (no free syncobj slots)");
            return 0;
        }

        return syncobj_slots_.insert(LucSyncObj{
            .in_use       = true,
            .has_fence    = false,
            .pre_signaled = (args.flags & DRM_SYNCOBJ_CREATE_SIGNALED) != 0,
        });
    }

    bool LucFile::syncobj_destroy(const u32 handle) {
        LucSyncObj* obj = syncobj_slots_.get(handle);
        if (!obj || !obj->in_use) {
            return false;
        }

        syncobj_slots_.remove(handle);
        return true;
    }

    bool LucFile::syncobj_bind_fence(const u32 handle, const u32 engine, const u64 target_seqno) {
        LucSyncObj* obj = syncobj_slots_.get(handle);
        if (!obj || !obj->in_use || obj->has_fence) {
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
    bool LucFile::syncobj_is_signaled_now(const LucFile::LucSyncObj& obj, IntelEngine* engine) {
        if (!obj.has_fence) {
            return obj.pre_signaled;
        }
        return engine && *engine->seqno_ptr_for_read() >= static_cast<u32>(obj.target_seqno);
    }

    int LucFile::syncobj_wait(
        const u32* handles, const u32 count_handles, const u32 flags,
        const i64 timeout_ns, u32* out_first_signaled
    ) {
        if (!handles || count_handles == 0) {
            return -1;
        }

        for (u32 i = 0; i < count_handles; ++i) {
            const LucSyncObj* obj = syncobj_slots_.get(handles[i]);
            if (!obj || !obj->in_use) {
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
                const LucSyncObj& obj = *syncobj_slots_.get(handles[i]);
                IntelEngine* engine = obj.has_fence ? device_.engine_for_class(obj.engine) : nullptr;

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

    bool LucFile::syncobj_reset(const u32* handles, const u32 count_handles) {
        if (!handles) {
            return false;
        }

        for (u32 i = 0; i < count_handles; ++i) {
            const LucSyncObj* obj = syncobj_slots_.get(handles[i]);
            if (!obj || !obj->in_use) {
                return false;
            }
        }

        for (u32 i = 0; i < count_handles; ++i) {
            LucSyncObj& obj = *syncobj_slots_.get(handles[i]);
            obj.has_fence = false;
            obj.pre_signaled = false;
            obj.engine = 0;
            obj.target_seqno = 0;
        }
        return true;
    }

    bool LucFile::syncobj_signal(const u32* handles, const u32 count_handles) {
        if (!handles) {
            return false;
        }

        for (u32 i = 0; i < count_handles; ++i) {
            const LucSyncObj* obj = syncobj_slots_.get(handles[i]);
            if (!obj || !obj->in_use) {
                return false;
            }
        }

        for (u32 i = 0; i < count_handles; ++i) {
            LucSyncObj& obj = *syncobj_slots_.get(handles[i]);
            obj.has_fence = false;
            obj.pre_signaled = true;
        }
        return true;
    }

    bool LucFile::query_gem_object(const u32 handle, GemObjectInfo* out) const {
        if (!out) {
            return false;
        }

        const GemObject* obj = lookup_gem(handle);
        if (!obj || obj->is_userptr) {
            // Userptr objects have no kernel-owned phys backing (see
            // vm_bind()'s MAP_USERPTR path) — nothing for mmap() to map yet.
            return false;
        }

        *out = GemObjectInfo{.phys_addr = obj->phys_addr, .size = obj->size};
        return true;
    }

    bool LucFile::gem_mmap_offset(const lucifer_gem_mmap_offset& args, u64* out_offset) {
        if (!out_offset) {
            return false;
        }

        const GemObject* obj = lookup_gem(args.handle);
        if (!obj || obj->is_userptr) {
            Log::log_dbc("intel-gpu: GEM_MMAP_OFFSET failed (bad handle)");
            return false;
        }

        *out_offset = static_cast<u64>(args.handle) * PAGE_SIZE;
        return true;
    }

    u32 LucFile::gem_handle_from_mmap_offset(const u64 offset) const {
        if (offset == 0 || offset % PAGE_SIZE != 0) {
            return 0;
        }

        const u64 handle = offset / PAGE_SIZE;
        if (handle == 0 || handle > MAX_LUCIFER_GEM_OBJECTS) {
            return 0;
        }

        return static_cast<u32>(handle);
    }

    kernel::vm::VmBackingObject* LucFile::get_backing_object(const u64 offset) const {
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

    int LucFile::ioctl(const u32 request, void* arg) {
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


            auto res = syncobj_wait(handles, wait->count_handles, wait->flags,
                                10000000, &wait->first_signaled);

            if (res == -ETIME) {
                IntelEngine* engine = device_.engine_for_class(0);
                if (engine) {
                    engine->dump_error_state("sync timeout");
                }
            }

            return res;
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

                out->device_id = device_.pci_cfg()->device_id;
                out->revision = device_.pci_cfg()->revision_id;

                IntelGpuDevice::FuseTopology topo = device_.query_fuse_topology();
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

                IntelGpuDevice::FuseTopology topo = device_.query_fuse_topology();

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

                out->total_size = device_.ggtt().usable_size_bytes();

                const u64 used_pages = static_cast<u64>(device_.ggtt().persistent_used_pages()) +
                    static_cast<u64>(device_.ggtt().transient_used_pages());

                out->used = used_pages * PAGE_SIZE;
                break;
            }
            case LUCIFER_QUERY_PCI_INFO: {
                auto* out = reinterpret_cast<lucifer_query_pci_info*>(query->data);

                out->domain = device_.pci_id().domain;
                out->bus = device_.pci_id().bus;
                out->dev = device_.pci_id().device;
                out->func = device_.pci_id().function;

                out->vendor_id = device_.pci_cfg()->vendor_id;
                out->device_id = device_.pci_cfg()->device_id;
                out->subsystem_vendor_id = device_.pci_cfg()->subsystem_vendor_id;
                out->subsystem_device_id = device_.pci_cfg()->subsystem_id;
                out->revision = device_.pci_cfg()->revision_id;

                out->name[0] = '\0';
                if (device_.get_kd() && device_.get_kd()->name) {
                    strncpy(out->name, device_.get_kd()->name, sizeof(out->name) - 1);
                    out->name[sizeof(out->name) - 1] = '\0';
                }
                break;
            }
            default: ;
        }

        query->size = expected_size;

        return 0;
    }
} // namespace gpu::intel::core

