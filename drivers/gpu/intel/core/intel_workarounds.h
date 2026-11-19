//
// Created by linus on 16.11.26.
//

#ifndef VESPERAOS_INTEL_WORKAROUNDS_H
#define VESPERAOS_INTEL_WORKAROUNDS_H

#include <vespera/types.h>

namespace gpu::intel::core::wa {
    bool enable_all_mmio_workarounds(volatile u8* mmio, u32 mcr_selector = 0);

    bool apply_rcs_engine_workarounds(volatile u8* mmio, u32 mcr_selector = 0);

    bool apply_gt_workarounds(volatile u8* mmio, u32 mcr_selector = 0);

    u32* emit_ctx_lri(u32* cs);
}

#endif //VESPERAOS_INTEL_WORKAROUNDS_H
