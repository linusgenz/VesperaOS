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
#include <vespera/scheduling.h>
#include <vespera/time.h>

#include "intel_engine.h"
#include "intel_gem_backing.h"
#include "intel_gpu_device.h"
#include "../kernel/units/unit.h"
#include "../regs/lrc_layout.h"

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

        for (usize i = 0; i < LUCIFER_NUM_ENGINE_CLASSES; i++) {
            const auto& ctx = engine_contexts_[i];
            if (ctx.sw_context_id == 0) continue;

            if (const IntelEngine* engine = device_.engine_for_class(i)) {
                engine->lrc_free(ctx);
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
        SpinlockGuard guard(handle_lock_);

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
        SpinlockGuard guard(handle_lock_);

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
        SpinlockGuard guard(handle_lock_);

        // SlotTable<IntelPpgtt*>::get() const returns IntelPpgtt* const*
        // (pointer-to-const-pointer-to-non-const-IntelPpgtt): the slot
        // itself can't be reassigned through this handle, but the
        // IntelPpgtt it points at is still mutable, same as the raw
        // vm_slots_[vm_id - 1] read this replaces.
        IntelPpgtt* const* slot = vm_slots_.get(vm_id);
        return slot ? *slot : nullptr;
    }

    u32 LucFile::gem_create(const lucifer_gem_create& args) {
        SpinlockGuard guard(handle_lock_);

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
        SpinlockGuard guard(handle_lock_);

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
        SpinlockGuard guard(handle_lock_);

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

    bool LucFile::gem_madvise(const lucifer_gem_madvise& args, bool* out_retained) const {
        GemObject* obj = gem_get_ref(args.handle);
        if (!obj || obj->is_userptr) {
            if (obj) obj->dec_ref();
            Log::log_dbc("intel-gpu: GEM_MADVISE failed (bad handle)");
            return false;
        }

        bool result_ok = true;

        if (args.state == LUCIFER_MADVICE_DONT_NEED) {
            // WE HAVE TO VERIFY THAT THIS IS ALREADY UNMAPPED
            if (!obj->purged) {
                /*const usize page_count = (obj->size + PAGE_SIZE - 1) / PAGE_SIZE;
                kernel::memory::free_pages_phys(obj->phys_addr, page_count);
                obj->phys_addr = phys_addr_t{};
                obj->purged = true;*/
            }

            *out_retained = false; // DONTNEED never reports resident
        } else if (obj->purged) {
            // LUCIFER_MADVICE_WILL_NEED, but backing store is gone and this
            // bring-up path never reallocates it -- caller
            // (lucifer_bo_madvise()) is expected to see retained == false
            // and recreate the BO.
            *out_retained = false;
        } else {
            *out_retained = true; // was never purged, still live
        }

        obj->dec_ref();
        return result_ok;
    }

    GemObject* LucFile::lookup_gem(const u32 handle) const {
        SpinlockGuard guard(handle_lock_);

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
        SpinlockGuard guard(handle_lock_);

        GemObject* const* slot = gem_slots_.get(handle);
        if (!slot || !*slot) {
            return nullptr;
        }
        (*slot)->inc_ref();
        return *slot;
    }

    bool LucFile::vm_bind(const lucifer_vm_bind& args) const {
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

        GemObject* obj = gem_get_ref(args.handle);
        if (!obj || obj->is_userptr || obj->purged) {
            if (obj) obj->dec_ref();
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

        const bool ok = vm->insert_range(make_gfx(args.addr), phys_start, args.range, caching, writable);
        obj->dec_ref();
        return ok;
    }

    EngineContext* LucFile::context_for_engine(IntelEngine* engine, const u32 engine_class) {
        SpinlockGuard guard(handle_lock_);
        return context_for_engine_locked(engine, engine_class);
    }

    EngineContext* LucFile::context_for_engine_locked(IntelEngine* engine, const u32 engine_class) {
        if (!engine || engine_class >= LUCIFER_NUM_ENGINE_CLASSES) {
            return nullptr;
        }

        EngineContext& ctx = engine_contexts_[engine_class];
        if (engine_contexts_initialized_[engine_class]) {
            return &ctx;
        }

        const u32 sw_context_id = (file_id_ << 4) | engine_class;

        // TODO(lucifer): LRC_SIZE_RCS vs LRC_SIZE_SMALL_ENGINE should come
        // from the engine itself (e.g. a virtual lrc_size_bytes() on
        // IntelEngine) once BCS/VCS/VECS are all wired through here --
        // hardcoding RCS's size for every class is a bring-up shortcut,
        // not something safe to leave once engine_class != RENDER is real.
        constexpr usize lrc_size_bytes = LRC_SIZE_RCS;

        if (!engine->ensure_execlist_mode_enabled()) {
            Log::log_dbc("intel-gpu: context_for_engine failed (ensure_execlist_mode_enabled, engine_class=%u)",
                         engine_class);
            return nullptr;
        }

        if (!engine->lrc_alloc_and_init(ctx, lrc_size_bytes, sw_context_id)) {
            Log::log_dbc("intel-gpu: context_for_engine failed (lrc_alloc_and_init, engine_class=%u)", engine_class);
            return nullptr;
        }

        engine_contexts_initialized_[engine_class] = true;
        return &ctx;
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

        EngineContext* ctx = context_for_engine(engine, args.engine);
        if (!ctx) {
            Log::log_dbc("intel-gpu: EXEC failed (context_for_engine)");
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
        {
            SpinlockGuard guard(handle_lock_);
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

        //vm->dump_batch_buffer(make_gfx(args.batch_addr), args.batch_len);

        u32 seqno = 0;
        if (!engine->submit_or_wait(make_gfx(args.batch_addr), args.batch_len, &seqno, vm, *ctx)) {
            Log::log_dbc("intel-gpu: EXEC failed (submit_or_wait)");
            return false;
        }

        args.out_seqno = seqno;

        for (u32 i = 0; i < args.num_syncs; ++i) {
            if (syncs[i].flags & LUCIFER_SYNC_FLAG_SIGNAL) {
                syncobj_bind_fence(syncs[i].handle, args.engine, seqno);
            }
        }

        Log::log_dbc("returning to userspace from submit");
        return true;
    }

    u32 LucFile::syncobj_create(const drm_syncobj_create& args) {
        SpinlockGuard guard(handle_lock_);

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
        SpinlockGuard guard(handle_lock_);

        LucSyncObj* obj = syncobj_slots_.get(handle);
        if (!obj || !obj->in_use) {
            return false;
        }

        syncobj_slots_.remove(handle);
        return true;
    }

    bool LucFile::syncobj_bind_fence(const u32 handle, const u32 engine, const u64 target_seqno) {
        SpinlockGuard guard(handle_lock_);

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
    bool LucFile::syncobj_is_signaled_now(const LucSyncObj& obj, IntelEngine* engine) {
        if (!obj.has_fence) {
            return obj.pre_signaled;
        }

        EngineContext* ctx = engine ? context_for_engine_locked(engine, obj.engine) : nullptr;
        return ctx && *engine->seqno_ptr_for_read(*ctx) >= static_cast<u32>(obj.target_seqno);
    }

    int LucFile::syncobj_wait_poll_once(
        const u32* handles, const u32 count_handles, const bool wait_all,
        u32* out_first_signaled, u32* out_signaled_count
    ) {
        SpinlockGuard guard(handle_lock_);

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
                if (!wait_all) {
                    if (out_first_signaled) *out_first_signaled = i;
                    *out_signaled_count = signaled_count;
                    return 0;
                }
            }
        }

        *out_signaled_count = signaled_count;
        return 0;
    }


    int LucFile::syncobj_wait_poll(
        const u32* handles, const u32 count_handles, const bool wait_all,
        const i64 timeout_ns, u32* out_first_signaled
    ) {
        const bool infinite = timeout_ns < 0;
        const u64 deadline_ns = infinite ? 0 : kernel::time::get_uptime_ns() + static_cast<u64>(timeout_ns);
        const i64 poll_interval_us = 500;

        while (true) {
            u32 signaled_count = 0;
            const int rc = syncobj_wait_poll_once(handles, count_handles, wait_all, out_first_signaled,
                                                   &signaled_count);
            if (rc != 0) return rc;
            if (!wait_all && signaled_count > 0) return 0;
            if (wait_all && signaled_count == count_handles) return 0;

            if (!infinite && kernel::time::get_uptime_ns() >= deadline_ns) {
                return -ETIME;
            }

            kernel::time::sleep_us(poll_interval_us);
        }
    }

    int LucFile::syncobj_wait(
        const u32* handles, const u32 count_handles, const u32 flags,
        const i64 timeout_ns, u32* out_first_signaled
    ) {
        if (!handles || count_handles == 0) {
            return -1;
        }

        {
            SpinlockGuard guard(handle_lock_);
            for (u32 i = 0; i < count_handles; ++i) {
                const LucSyncObj* obj = syncobj_slots_.get(handles[i]);
                if (!obj || !obj->in_use) {
                    Log::log_dbc("intel-gpu: SYNCOBJ_WAIT failed (bad handle=%u)", handles[i]);
                    return -1;
                }
            }
        }

        const bool wait_all = (flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL) != 0;
        IntelEngine* wait_engines[LUCIFER_NUM_ENGINE_CLASSES] = {};
        u32 num_wait_engines = 0;

        {
            SpinlockGuard guard(handle_lock_);
            for (u32 i = 0; i < count_handles; ++i) {
                const LucSyncObj& obj = *syncobj_slots_.get(handles[i]);
                if (!obj.has_fence) continue;

                IntelEngine* engine = device_.engine_for_class(obj.engine);
                if (!engine) continue;

                bool already_have = false;
                for (u32 j = 0; j < num_wait_engines; ++j) {
                    if (wait_engines[j] == engine) {
                        already_have = true;
                        break;
                    }
                }
                if (already_have) continue;

                if (num_wait_engines >= LUCIFER_NUM_ENGINE_CLASSES) {
                    // Shouldn't happen (see comment above), but if the
                    // engine count ever grows past our fixed array,
                    // degrade to polling rather than drop an engine from
                    // the wait set and risk missing its wakeup.
                    return syncobj_wait_poll(handles, count_handles, wait_all, timeout_ns, out_first_signaled);
                }
                wait_engines[num_wait_engines++] = engine;
            }
        }

        Unit* cur = kernel::scheduling::get_current_unit();
        if (!cur || cur->is_idle) {
            return syncobj_wait_poll(handles, count_handles, wait_all, timeout_ns, out_first_signaled);
        }

        const bool infinite = timeout_ns < 0;
        const u64 deadline_ns = infinite ? 0 : kernel::time::get_uptime_ns() + static_cast<u64>(timeout_ns);
        const u8 cpu_id = cur->cpu_id;

        while (true) {
            u32 signaled_count = 0;
            int rc = syncobj_wait_poll_once(handles, count_handles, wait_all, out_first_signaled, &signaled_count);
            if (rc != 0) return rc;
            if (!wait_all && signaled_count > 0) return 0;
            if (wait_all && signaled_count == count_handles) return 0;

            if (num_wait_engines == 0) {
                return syncobj_wait_poll(handles, count_handles, wait_all, timeout_ns, out_first_signaled);
            }

            for (u32 j = 0; j < num_wait_engines; ++j) {
                wait_engines[j]->waiters.add_wait(cur);
            }

            if (!infinite) {
                cur->sleep_context.wakeup_ns = deadline_ns;
                kernel::scheduling::add_blocked_unit(cur, cpu_id);
            }

            kernel::scheduling::yield();

            rc = syncobj_wait_poll_once(handles, count_handles, wait_all, out_first_signaled, &signaled_count);
            if (rc != 0) return rc;
            if (!wait_all && signaled_count > 0) return 0;
            if (wait_all && signaled_count == count_handles) return 0;

            if (!infinite && kernel::time::get_uptime_ns() >= deadline_ns) {
                bool any_lost_race = false;
                for (u32 j = 0; j < num_wait_engines; ++j) {
                    if (!wait_engines[j]->waiters.remove(cur)) {
                        any_lost_race = true;
                    }
                }
                if (!any_lost_race) {
                    return -ETIME;
                }
            }

            // Not yet signaled, not timed out (or lost a removal race) --
            // loop back and park again.
        }
    }

    bool LucFile::syncobj_reset(const u32* handles, const u32 count_handles) {
        if (!handles) {
            return false;
        }

        SpinlockGuard guard(handle_lock_);

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

        SpinlockGuard guard(handle_lock_);

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

        // gem_get_ref() (not lookup_gem()) so a concurrent gem_close() on
        // this handle can't free the object between the lookup and the
        // phys_addr/size reads below.
        GemObject* obj = gem_get_ref(handle);
        if (!obj || obj->is_userptr) {
            // Userptr objects have no kernel-owned phys backing (see
            // vm_bind()'s MAP_USERPTR path) — nothing for mmap() to map yet.
            if (obj) obj->dec_ref();
            return false;
        }

        *out = GemObjectInfo{.phys_addr = obj->phys_addr, .size = obj->size};
        obj->dec_ref();
        return true;
    }

    bool LucFile::gem_mmap_offset(const lucifer_gem_mmap_offset& args, u64* out_offset) {
        if (!out_offset) {
            return false;
        }

        GemObject* obj = gem_get_ref(args.handle);
        if (!obj || obj->is_userptr) {
            if (obj) obj->dec_ref();
            Log::log_dbc("intel-gpu: GEM_MMAP_OFFSET failed (bad handle)");
            return false;
        }
        obj->dec_ref();

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

            return syncobj_wait(handles, wait->count_handles, wait->flags,
                                wait->deadline_nsec, &wait->first_signaled);
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
