//
// Created by linus on 17.11.26.
//

#ifndef VESPERAWORKSPACE_INTEL_DBG_H
#define VESPERAWORKSPACE_INTEL_DBG_H

#include "vespera/types.h"

namespace gpu::intel::core::dbg {
    constexpr u32  DELAY_US                     = 2000;
    constexpr bool EXP_BO_POOL                  = false; // T4
    constexpr bool EXP_NO_INDIRECT_STATE_DISABLE = false; // T5
    constexpr bool LOG_BATCH                    = true;  // T2
}

namespace gpu::intel::core::dbg_pool {
    struct Entry { phys_addr_t phys; usize pages; };
    inline Entry g_e[16];
    inline u32   g_n = 0;
    inline u8    g_lock = 0;

    inline void lock()   { while (__atomic_test_and_set(&g_lock, __ATOMIC_ACQUIRE)) asm volatile("pause"); }
    inline void unlock() { __atomic_clear(&g_lock, __ATOMIC_RELEASE); }

    inline bool push(const phys_addr_t p, const usize pages) {
        lock(); const bool ok = g_n < 16; if (ok) g_e[g_n++] = Entry{p, pages}; unlock(); return ok;
    }
    inline bool pop(const usize pages, phys_addr_t* out) {
        lock();
        for (u32 i = 0; i < g_n; i++)
            if (g_e[i].pages == pages) { *out = g_e[i].phys; g_e[i] = g_e[--g_n]; unlock(); return true; }
        unlock(); return false;
    }
}

#endif //VESPERAWORKSPACE_INTEL_DBG_H
