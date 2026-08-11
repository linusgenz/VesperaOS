// state_allocator.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 11.08.26.
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
#ifndef VESPERAOS_STATE_ALLOCATOR_H
#define VESPERAOS_STATE_ALLOCATOR_H

#include <vespera/mm/addr.h>
#include <vespera/types.h>

#include <gpu/intel/core/ggtt_allocator.h>

namespace gpu::intel::core {

    // Backs everything a 3D pipeline binds through SURFACE_STATE_BASE_ADDRESS,
    // DYNAMIC_STATE_BASE_ADDRESS and INSTRUCTION_STATE_BASE_ADDRESS.
    //
    //   - persistent zone: bump allocator, never freed. Shader kernels,
    //     static SCISSOR_RECT/SF_CLIP_VIEWPORT/CC_VIEWPORT/BLEND_STATE,
    //     RENDER_SURFACE_STATE, BINDING_TABLE_STATE — everything you set up
    //     once at pipeline-init and never touch again.
    //
    //   - transient zone: NOT a freelist (unlike GgttAllocator) but a fixed
    //     ring of `frame_count` slots. Transient state has one very regular
    //     lifetime pattern: allocated for frame N, read by the batch you
    //     submit for frame N, safe to reuse once that batch's seqno has
    //     signaled. A ring models that exactly, with no fragmentation and
    //     no free-list bookkeeping. This is where a rotating cube's
    //     per-frame transform/constant buffer lives — begin_frame() up
    //     front, alloc_transient() to write this frame's matrix,
    //     end_frame(seqno) after submitting, repeat. With frame_count >= 2
    //     the CPU can prepare frame N+1's state while the GPU is still
    //     consuming frame N's, instead of a hard seqno_wait every frame.
    //
    class StateAllocator {
       public:
        static constexpr u32 MAX_FRAME_SLOTS = 4;

        struct StateAlloc {
            u32 offset = U32_MAX;   // byte offset from base_gfx_addr(), feed straight into *_POINTERS / *_BASE_ADDRESS commands
            u8* cpu_ptr = nullptr;  // CPU pointer to the same location, write here (then flush_range or use write_*)

            [[nodiscard]] bool valid() const { return offset != U32_MAX; }
        };

        StateAllocator() = default;

        /**
         * @brief One-time setup. Takes one page-granular allocation out of
         *        the GGTT and carves it into a persistent bump zone plus
         *        `frame_count` transient ring slots.
         *
         * @param persistent_bytes Capacity of the persistent zone. Size it
         *        for everything the pipeline sets up once: shader ISA +
         *        static dynamic state + surface state + binding table.
         * @param frame_bytes      Capacity of ONE transient ring slot. Size
         *        it for the state a single frame writes (e.g. one MVP
         *        constant buffer, or several if you're juggling multiple
         *        objects).
         * @param frame_count      Number of ring slots (>=1, <=
         *        MAX_FRAME_SLOTS). 1 = single-buffered (must fence-wait
         *        every frame, same behavior as before). 2-3 = double/triple
         *        buffering, recommended for anything animated.
         * @return false if frame_count is out of range or the GGTT couldn't
         *         satisfy the resulting page request.
         */
        [[nodiscard]] bool init(
            GgttAllocator& ggtt, u32 persistent_bytes, u32 frame_bytes, u32 frame_count = 3,
            u8 pat_index = MOCS_UNCACHED
        );

        [[nodiscard]] StateAlloc alloc_persistent(u32 size, u32 align);

        // Bump-allocate + memcpy + cache-flush in one call. Prefer this over
        // alloc_persistent() + manual memcpy/clflush at call sites.
        template <typename T>
        StateAlloc write_persistent(const T& data, u32 align = alignof(T)) {
            StateAlloc a = alloc_persistent(sizeof(T), align);
            if (a.cpu_ptr) {
                memcpy(a.cpu_ptr, &data, sizeof(T));
                flush_range(a.cpu_ptr, sizeof(T));
            }
            return a;
        }

        [[nodiscard]] u32 persistent_used_bytes() const { return persistent_next_; }
        [[nodiscard]] u32 persistent_capacity_bytes() const { return persistent_capacity_; }

        /**
         * @brief Advance to the next ring slot and rewind its bump pointer.
         *        Call once per frame, before the frame's first
         *        alloc_transient()/write_transient().
         * @return The seqno that must have already signaled before you
         *         overwrite this slot's previous contents (0 if this slot
         *         has never been used, e.g. the first `frame_count`
         *         frames). Caller is responsible for the actual wait
         *         (seqno_wait / completion_flag) — this class stays engine-
         *         agnostic, same as GgttAllocator does no submission work
         *         itself.
         */
        [[nodiscard]] u64 begin_frame();

        [[nodiscard]] StateAlloc alloc_transient(u32 size, u32 align);

        template <typename T>
        StateAlloc write_transient(const T& data, u32 align = alignof(T)) {
            StateAlloc a = alloc_transient(sizeof(T), align);
            if (a.cpu_ptr) {
                memcpy(a.cpu_ptr, &data, sizeof(T));
                flush_range(a.cpu_ptr, sizeof(T));
            }
            return a;
        }

        /**
         * @brief Record the seqno of the batch that reads everything
         *        allocated in the *current* slot since begin_frame(). Call
         *        right after ring_flush()/emit_flush() for that frame's
         *        batch, with the same seqno you pass to seqno_wait.
         */
        void end_frame(u64 seqno);

        [[nodiscard]] u32 frame_used_bytes() const { return frame_cursor_; }
        [[nodiscard]] u32 frame_capacity_bytes() const { return frame_capacity_; }
        [[nodiscard]] u32 frame_count() const { return frame_count_; }
        [[nodiscard]] u32 current_frame_slot() const { return frame_slot_; }

        [[nodiscard]] u64 base_gfx_addr() const { return gfx_raw(gfx_base_); }
        [[nodiscard]] void* base_cpu_addr() const { return virt_ptr(cpu_base_); }
        [[nodiscard]] u32 total_pages() const { return pages_; }

        static void flush_range(const void* ptr, usize size);

       private:
        virt_addr_t cpu_base_{};
        gfx_addr_t gfx_base_{};
        u32 pages_ = 0;

        u32 persistent_capacity_ = 0;
        u32 persistent_next_ = 0;

        u32 frame_capacity_ = 0;
        u32 frame_count_ = 0;
        u32 frame_slot_ = 0;
        u32 frame_cursor_ = 0;
        bool frame_started_ = false;

        u64 slot_fence_[MAX_FRAME_SLOTS] = {};
    };

}  // namespace gpu::intel::core

#endif  // VESPERAOS_STATE_ALLOCATOR_H
