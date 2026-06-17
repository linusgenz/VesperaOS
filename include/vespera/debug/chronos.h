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

#ifndef VESPERAOS_CHRONOS_H
#define VESPERAOS_CHRONOS_H

#include <uapi/vespera/chronos.h>

namespace kernel::chronos {
    /**
     * Initialise the framework.  Must be called after ClockManager::init()
     * and before any checkpoint() call.
     */
    void init();

    /**
     * Record a checkpoint from kernel code.
     *
     * @param phase  Subsystem name (e.g. "PCI").  Pass nullptr to inherit the
     *               current push_phase() context.
     * @param label  Checkpoint label  (e.g. "enumerate_done").
     */
    void checkpoint(const char* phase, const char* label);

    /**
     * Record a checkpoint that was submitted by a userspace realm via the
     * sys_chronos_checkpoint syscall.  The kernel fills in ts_ns / cpu_id;
     * the realm_id comes from the currently scheduled realm.
     *
     * @param realm_id  ID of the originating realm (must be > 0).
     * @param cp        Validated, kernel-local copy of chronos_user_checkpoint_t.
     */
    void checkpoint_from_user(u32 realm_id, const ::chronos::UserCheckpoint& cp);

    /** Push a named phase onto the thread-local phase stack. */
    void push_phase(const char* phase);

    /** Pop the innermost phase. No-op if the stack is already empty. */
    void pop_phase();

    /** Dump the full event timeline via DBC. */
    void dump_dbc();

    /** Dump a per-phase aggregated summary (sorted by total cost) via DBC. */
    void dump_summary_dbc();

    struct PhaseScope {
        explicit PhaseScope(const char* phase) { push_phase(phase); }
        ~PhaseScope() { pop_phase(); }

        PhaseScope(const PhaseScope&) = delete;
        PhaseScope& operator=(const PhaseScope&) = delete;
    };

    struct ScopedTimer {
        const char* phase;
        const char* label;

        ScopedTimer(const char* p, const char* l) : phase(p), label(l) {
        }

        ~ScopedTimer() { checkpoint(phase, label); }

        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;
    };
} // namespace kernel::chronos


#ifndef VESPERA_CHRONOS
#  define VESPERA_CHRONOS 1
#endif

#if VESPERA_CHRONOS

#  define CHRONOS_CP(label) \
       ::kernel::chronos::checkpoint(nullptr, (label))

#  define CHRONOS_CP_PHASE(phase, label) \
       ::kernel::chronos::checkpoint((phase), (label))

#  define CHRONOS_PHASE_SCOPE(phase) \
       ::kernel::chronos::PhaseScope \
           _chronos_phase_##__LINE__{(phase)}

#  define CHRONOS_TIMER_SCOPE(label) \
       ::kernel::chronos::ScopedTimer \
           _chronos_timer_##__LINE__{nullptr, (label)}

#  define CHRONOS_TIMER_SCOPE_PHASE(phase, label) \
       ::kernel::chronos::ScopedTimer \
           _chronos_timer_##__LINE__{(phase), (label)}

#  define CHRONOS_DUMP()         ::kernel::chronos::dump_dbc()
#  define CHRONOS_DUMP_SUMMARY() ::kernel::chronos::dump_summary_dbc()

#else

#  define CHRONOS_CP(label)                    do {} while (0)
#  define CHRONOS_CP_PHASE(phase, label)       do {} while (0)
#  define CHRONOS_PHASE_SCOPE(phase)
#  define CHRONOS_TIMER_SCOPE(label)
#  define CHRONOS_TIMER_SCOPE_PHASE(phase, label)
#  define CHRONOS_DUMP()                       do {} while (0)
#  define CHRONOS_DUMP_SUMMARY()               do {} while (0)

#endif /* VESPERA_CHRONOS */

#endif /* VESPERAOS_CHRONOS_H */
