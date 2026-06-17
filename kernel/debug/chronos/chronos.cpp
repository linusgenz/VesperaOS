// chronos.cpp
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

#include "vespera/debug/chronos.h"

#include <time/clock_manager.h>
#include <vespera/log.h>
#include <klib/string.h>
#include "vespera/cpu/cpu_manager.h"

bool flag = false;
namespace kernel::chronos {
    static ::chronos::Event s_events[::chronos::MAX_EVENTS];

    static u32 s_count = 0;
    static u64 s_boot_start = 0;

    static constexpr usize DELTA_SLOTS = 64;
    static u64 s_last_ts[DELTA_SLOTS] = {};

    static constexpr usize PHASE_STACK_DEPTH = 16;
    static const char* s_phase_stack[PHASE_STACK_DEPTH] = {};
    static usize s_phase_depth = 0;

    static const char* current_phase() {
        if (s_phase_depth == 0) return "kernel";
        return s_phase_stack[s_phase_depth - 1];
    }

    static u32 alloc_slot() {
        return __atomic_fetch_add(&s_count, 1u, __ATOMIC_RELAXED);
    }

    static u64 delta_for(u32 realm_id) {
        usize slot = (realm_id < DELTA_SLOTS) ? realm_id : 0;
        u64 ts = time::clock_manager::read_ns();
        u64 prev = s_last_ts[slot];

        s_last_ts[slot] = ts;
        return ts - (prev ? prev : s_boot_start);
    }

    static void write_event(
        u32 slot,
        const char* phase, const char* label,
        u64 ts_ns, u64 delta_ns,
        u32 cpu_id, u32 realm_id,
        ::chronos::EventSource src
    ) {
        auto& ev = s_events[slot];
        strncpy(ev.phase, phase, ::chronos::PHASE_MAX - 1);
        ev.phase[::chronos::PHASE_MAX - 1] = '\0';
        strncpy(ev.label, label, ::chronos::LABEL_MAX - 1);
        ev.label[::chronos::LABEL_MAX - 1] = '\0';
        ev.ts_ns = ts_ns;
        ev.delta_ns = delta_ns;
        ev.cpu_id = cpu_id;
        ev.realm_id = realm_id;
        ev.source = static_cast<u8>(src);
        memset(ev._pad, 0, sizeof(ev._pad));
    }

    /** Renders nanoseconds as "XXXX.XXXms\0" into buf (at least 16 bytes). */
    static void fmt_ns(u64 ns, char* buf, usize len) {
        u64 us = ns / 1000u;
        u64 ms = us / 1000u;
        u64 us_frac = us % 1000u;
        snprintf(buf, len, "%llu.%03llums", ms, us_frac);
    }

    void init() {
        s_boot_start = time::clock_manager::read_ns();
        s_count = 0;
        s_phase_depth = 0;
        memset(s_last_ts, 0, sizeof(s_last_ts));
        memset(s_events, 0, sizeof(s_events));
        memset(s_phase_stack, 0, sizeof(s_phase_stack));

        /* Seed all delta slots to boot_start so first delta is from t=0. */
        for (usize i = 0; i < DELTA_SLOTS; ++i)
            s_last_ts[i] = s_boot_start;
    }

    void push_phase(const char* phase) {
        if (s_phase_depth < PHASE_STACK_DEPTH)
            s_phase_stack[s_phase_depth++] = phase;
    }

    void pop_phase() {
        if (s_phase_depth > 0)
            --s_phase_depth;
    }

    void checkpoint(const char* phase, const char* label) {
        if (!flag) return;
        u32 slot = alloc_slot();
        if (slot >= ::chronos::MAX_EVENTS) return;

        const char* eff_phase = phase ? phase : current_phase();
        u64 ts = time::clock_manager::read_ns();
        u64 delta = ts - s_last_ts[0];
        s_last_ts[0] = ts;

        write_event(slot,
                    eff_phase, label,
                    ts - s_boot_start, delta,
                    cpu_manager::get_current_cpu_id(), 0u,
                    ::chronos::EventSource::Kernel);
    }

    void checkpoint_from_user(u32 realm_id, const ::chronos::UserCheckpoint& cp) {
        u32 slot = alloc_slot();
        if (slot >= ::chronos::MAX_EVENTS) return;

        u64 ts = time::clock_manager::read_ns();
        u64 delta = delta_for(realm_id);

        write_event(slot,
                    cp.phase, cp.label,
                    ts - s_boot_start, delta,
                    cpu_manager::get_current_cpu_id(), realm_id,
                    ::chronos::EventSource::User);
    }

    void dump_dbc() {
        u32 count = __atomic_load_n(&s_count, __ATOMIC_ACQUIRE);
        if (count > ::chronos::MAX_EVENTS)
            count = ::chronos::MAX_EVENTS;

        Log::log_dbc("\n[CHRONOS] ── %u events ───────────────────────────────\n",
                     count);
        Log::log_dbc("  %-3s  %-4s  %-13s  %-22s  %-20s  %s\n",
                     "SRC", "CPU", "T_boot", "Phase", "Label", "Delta");

        char t_buf[20], d_buf[20];
        for (u32 i = 0; i < count; ++i) {
            const auto& ev = s_events[i];
            fmt_ns(ev.ts_ns, t_buf, sizeof(t_buf));
            fmt_ns(ev.delta_ns, d_buf, sizeof(d_buf));

            const char* src =
                (ev.source == CHRONOS_SOURCE_KERNEL) ? "K" : "U";

            if (ev.source == CHRONOS_SOURCE_KERNEL) {
                Log::log_dbc("  [%s:%u]  %-13s  %-22s  %-20s  +%s",
                             src, ev.cpu_id,
                             t_buf, ev.phase, ev.label, d_buf);
            } else {
                Log::log_dbc("  [%s:r%u] %-13s  %-22s  %-20s  +%s",
                             src, ev.realm_id,
                             t_buf, ev.phase, ev.label, d_buf);
            }
        }
        Log::log_dbc("[CHRONOS] ─────────────────────────────────────────────\n");
    }

    void dump_summary_dbc() {
        u32 count = __atomic_load_n(&s_count, __ATOMIC_ACQUIRE);
        if (count > ::chronos::MAX_EVENTS)
            count = ::chronos::MAX_EVENTS;

        struct PhaseStat {
            char name[::chronos::PHASE_MAX];
            u64 total_ns;
            u32 hits;
            u32 realm_id;
        };

        static constexpr usize MAX_PHASES = 64;
        static PhaseStat stats[MAX_PHASES];
        usize nstats = 0;
        memset(stats, 0, sizeof(stats));

        auto find_or_add = [&](const char* name, u32 realm_id) -> PhaseStat* {
            for (usize i = 0; i < nstats; ++i) {
                if (strcmp(stats[i].name, name) == 0 &&
                    stats[i].realm_id == realm_id)
                    return &stats[i];
            }
            if (nstats >= MAX_PHASES) return nullptr;
            auto& s = stats[nstats++];
            strncpy(s.name, name, ::chronos::PHASE_MAX - 1);
            s.name[::chronos::PHASE_MAX - 1] = '\0';
            s.total_ns = 0;
            s.hits = 0;
            s.realm_id = realm_id;
            return &s;
        };

        for (u32 i = 0; i < count; ++i) {
            const auto& ev = s_events[i];
            if (auto* s = find_or_add(ev.phase, ev.realm_id)) {
                s->total_ns += ev.delta_ns;
                s->hits++;
            }
        }

        /* Insertion-sort descending by total_ns (n is tiny). */
        for (usize i = 1; i < nstats; ++i) {
            PhaseStat key = stats[i];
            isize j = static_cast<isize>(i) - 1;
            while (j >= 0 && stats[j].total_ns < key.total_ns) {
                stats[j + 1] = stats[j];
                --j;
            }
            stats[j + 1] = key;
        }

        Log::log_dbc("\n[CHRONOS SUMMARY] ── Top phases ──────────────────────\n");
        Log::log_dbc("  %-22s  %-6s  %-14s  %s\n",
                     "Phase", "Origin", "Total", "Events");

        char buf[20];
        for (usize i = 0; i < nstats; ++i) {
            fmt_ns(stats[i].total_ns, buf, sizeof(buf));
            const char* origin =
                (stats[i].realm_id == 0) ? "kernel" : "realm";

            if (stats[i].realm_id == 0) {
                Log::log_dbc("  %-22s  %-6s  %-14s  %u",
                             stats[i].name, origin, buf, stats[i].hits);
            } else {
                char rbuf[12];
                snprintf(rbuf, sizeof(rbuf), "r%u", stats[i].realm_id);
                Log::log_dbc("  %-22s  %-6s  %-14s  %u",
                             stats[i].name, rbuf, buf, stats[i].hits);
            }
        }
        Log::log_dbc("[CHRONOS SUMMARY] ────────────────────────────────────\n");
    }
} // namespace kernel::chronos
