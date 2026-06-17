// chronos.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.06.26.
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
#ifndef VESPLIB_CHRONOS_H
#define VESPLIB_CHRONOS_H

#include <vespera/chronos.h>
#include <string.h>

#include "sysstd.h"
#include "stdint.h"

#ifndef VESPERA_CHRONOS_USER
#define VESPERA_CHRONOS_USER 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if VESPERA_CHRONOS_USER

/**
 * Record a timing checkpoint.  Performs a single SYS_CHRONOS_CHECKPOINT
 * syscall.  Errors are silently discarded (profiling must not crash the app).
 *
 * @param phase  Subsystem / module name, e.g. "firmament"  (max 23 chars)
 * @param label  Checkpoint name, e.g. "icons_loaded"       (max 47 chars)
 */
static inline void chronos_checkpoint(const char* phase, const char* label) {
    chronos_user_checkpoint_t cp = {0};
    strncpy(cp.phase, phase, CHRONOS_PHASE_MAX - 1);
    strncpy(cp.label, label, CHRONOS_LABEL_MAX - 1);
    sys_chronos_checkpoint((uint64_t)(&cp), 0, 0, 0, 0, 0);
}

static inline void chronos_summary() {
    sys_chronos_summary(0,0,0,0,0,0);
}

#else

static inline void chronos_checkpoint(const char* phase, const char* label) {
    (void)phase;
    (void)label;
}

static inline void chronos_summary() {
    (void)
}

#endif /* VESPERA_CHRONOS_USER */

#ifdef __cplusplus
} /* extern "C" */
#endif

#if VESPERA_CHRONOS_USER

/** Record a checkpoint with an explicit phase name. */
#define CHRONOS_CP_PHASE(phase, label) \
       chronos_checkpoint((phase), (label))

/** Record a checkpoint inheriting no phase context (use in C, or short paths). */
#define CHRONOS_CP(label) \
       chronos_checkpoint("", (label))

#define CHRONOS_SUMMARY() \
       chronos_summary()


#else /* stripped */

#define CHRONOS_CP_PHASE(phase, label)  do {} while (0)
#define CHRONOS_CP(label)               do {} while (0)
#define CHRONOS_SUMMARY()               do {} while (0)

#endif /* VESPERA_CHRONOS_USER */

#ifdef __cplusplus
#if VESPERA_CHRONOS_USER

namespace chronos {
    /**
     * Scoped phase marker: records "<phase>::scope_begin" on construction
     * and "<phase>::scope_end" on destruction.
     *
     * Usage:
     *   {
     *       chronos::PhaseScope ps{"firmament.lvgl"};
     *       stella_init();
     *       // scope_end checkpoint here
     *   }
     */
    class PhaseScope {
    public:
        explicit PhaseScope(const char* phase) : phase_(phase) {
            chronos_checkpoint(phase_, "scope_begin");
        }

        ~PhaseScope() {
            chronos_checkpoint(phase_, "scope_end");
        }

        PhaseScope(const PhaseScope&) = delete;
        PhaseScope& operator=(const PhaseScope&) = delete;

    private:
        const char* phase_;
    };

    /**
     * Scoped timer: records a "<phase>::<label>" checkpoint on destruction.
     * The delta shown in the kernel dump equals the time spent in the scope.
     *
     * Usage:
     *   {
     *       chronos::ScopedTimer t{"firmament", "icon_load"};
     *       astra_load_icons();
     *       // checkpoint here → delta = time spent in astra_load_icons()
     *   }
     */
    class ScopedTimer {
    public:
        ScopedTimer(const char* phase, const char* label)
            : phase_(phase), label_(label) {
        }

        ~ScopedTimer() {
            chronos_checkpoint(phase_, label_);
        }

        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;

    private:
        const char* phase_;
        const char* label_;
    };
} // namespace chronos

#define CHRONOS_PHASE_SCOPE(phase) \
       ::chronos::PhaseScope _chronos_ps_##__LINE__{(phase)}

#define CHRONOS_TIMER_SCOPE(phase, label) \
       ::chronos::ScopedTimer _chronos_st_##__LINE__{(phase), (label)}

#else

namespace chronos {
    struct PhaseScope {
        explicit PhaseScope(const char*) {
        }
    };

    struct ScopedTimer {
        ScopedTimer(const char*, const char*) {
        }
    };
} // namespace chronos

#define CHRONOS_PHASE_SCOPE(phase)
#define CHRONOS_TIMER_SCOPE(phase, label)

#endif /* VESPERA_CHRONOS_USER */
#endif /* __cplusplus */


#endif //VESPLIB_CHRONOS_H
