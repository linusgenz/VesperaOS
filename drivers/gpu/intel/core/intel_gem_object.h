// intel_gem_object.h
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

#ifndef VESPERAOS_INTEL_GEM_OBJECT_H
#define VESPERAOS_INTEL_GEM_OBJECT_H

#include <vespera/mm/addr.h>
#include <vespera/mm/vm_backing.h>
#include <vespera/types.h>

#include "uapi/vespera/dev/lucifer_drm.h"

namespace gpu::intel::core {
    struct EngineContext;

    struct GemFenceRef {
        const EngineContext* ctx = nullptr;
        u32 seqno = 0;
        u8 engine_class = 0;
    };

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
    /// Still a struct, not a class: unlike LucFile, GemObject has no
    /// invariant beyond its own refcount (guarded by inc_ref()/dec_ref()
    /// themselves, not by hiding the other fields) -- every other field is
    /// legitimately just data callers read directly (size, phys_addr,
    /// is_userptr, ...), so there's nothing an access specifier would be
    /// protecting.
    struct GemObject {
        u64 size = 0;          ///< requested size in bytes (0 for userptr)
        u32 placement = 0;     ///< memory region bitmask, from GEM_CREATE
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

        GemFenceRef last_write{};

        /// Refs: the handle slot itself (dropped by gem_close()/LucFile
        /// teardown) plus one per live IntelGemBackingObject / LucDmaBuf.
        /// Pages are freed -- and the object deleted -- only when this
        /// reaches zero, so a handle closed while still mmap()'d or
        /// exported stays valid until every reference goes away.
        u32 refcount = 0;

        [[nodiscard]] phys_addr_t page_phys_at(const usize offset_in_bytes) const {
            if (is_userptr || purged || offset_in_bytes >= size) {
                return phys_addr_t{};
            }
            // GEM_CREATE's backing is contiguous -> no page table walk
            return phys_add(phys_addr, offset_in_bytes);
        }

        [[nodiscard]] CacheMode cache_mode() const {
            return (cpu_caching == LUCIFER_GEM_CPU_CACHING_WC)
                       ? CacheMode::WriteCombining
                       : CacheMode::WriteBack;
        }

        void inc_ref() {
            __atomic_add_fetch(&refcount, 1, __ATOMIC_RELAXED);
        }

        /// Returns true if this was the last reference (pages already
        /// freed and *this already destroyed by the time this returns --
        /// never touch the object again after a call that returns true).
        [[nodiscard]] bool dec_ref();
    };
} // namespace gpu::intel::core

#endif //VESPERAOS_INTEL_GEM_OBJECT_H
