//
// Created by Linus on 17.07.25.
//

#include <arch/x86_64/cpu/msr.h>
#include <klib/string.h>
#include <realm/realm.h>
#include <units/unit.h>
#include <vespera/log.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera/time.h>

#include "cpu/cpu_manager.h"
#include "cpu_scheduler.h"
#include "scheduler_types.h"

namespace kernel::scheduling {

    GlobalScheduler global_scheduler = {{}};

    void init(u32 num_cpus) {
        global_scheduler.num_cpus = num_cpus;
        global_scheduler.initialized = true;

        for (u32 i = 0; i < num_cpus; i++) {
            cpu_scheduler::init_cpu(i);
        }
    }

    void add_unit(Unit *unit) {
        if (!unit || !global_scheduler.initialized) return;

        const u8 cpu_id = unit->cpu_id;
        if (cpu_id >= global_scheduler.num_cpus) return;

        cpu_scheduler::add_unit_to_cpu(unit, cpu_id);
    }

    void remove_unit(Unit *unit) {
        if (!unit || !global_scheduler.initialized) return;

        const u8 cpu_id = unit->cpu_id;
        if (cpu_id >= global_scheduler.num_cpus) return;

        cpu_scheduler::remove_unit_from_cpu(unit, cpu_id);
    }

    void yield() {
        asm volatile("int $0x23");
    }

    void yield_cpu(u8 cpu_id, TrapFrame *frame) {
        cpu_scheduler::yield_cpu(cpu_id, frame);
    }

    void enable_on_cpu(u8 cpu_id) {
        if (cpu_id >= global_scheduler.num_cpus) return;

        cpu_scheduler::enable_cpu(cpu_id);
        yield();
    }

    void disable_on_cpu(u8 cpu_id) {
        if (cpu_id >= global_scheduler.num_cpus) return;

        cpu_scheduler::disable_cpu(cpu_id);
    }

    void add_blocked_unit(Unit *unit, u8 cpu_id) {
        cpu_scheduler::add_blocked_unit(unit, cpu_id);
    }

    void remove_blocked_unit(Unit* unit) {
        cpu_scheduler::remove_blocked_unit(unit);
    }

    bool is_curent_cpu_enabled() {
        const u8 cpu_id = cpu_manager::get_current_cpu_id();
        return cpu_scheduler::is_cpu_enabled(cpu_id);
    }

    bool set_fs_base(u64 addr) {
        Unit *u = get_current_unit();
        if (!u) return false;
        u->context.fs_base = addr;
        wrmsr(MSR_FS_BASE, addr);
        return true;
    }

    bool get_fs_base(u64 *out) {
        const Unit *u = get_current_unit();
        if (!u || !out) return false;
        *out = u->context.fs_base;
        return true;
    }

    bool is_current_unit_idle() {
        const Unit *u = get_current_unit();
        return !u || u->is_idle;
    }

    Unit *get_current_unit() {
        const u32 cpu_id = cpu_manager::get_current_cpu_id();
        if (!global_scheduler.cpus[cpu_id].scheduler_enabled) return nullptr;
        return cpu_scheduler::get_current_unit_on_cpu(cpu_id);
    }

    Realm *get_current_realm() {
        const Unit *u = get_current_unit();
        if (!u) return nullptr;
        return u->parent;
    }

    u64 get_realm_cpu_time_ns(const RealmId realm_id) {
        const Realm *realm = RealmManager::get(realm_id);
        if (!realm) return 0;

        const Unit *current = get_current_unit();
        u64 total_ns = 0;

        for (const Unit *u = realm->unit_list; u; u = u->next) {
            total_ns += u->cpu_time_ns;
            if (u == current && u->run_start_ns != 0) total_ns += time::get_uptime_ns() - u->run_start_ns;
        }

        return total_ns;
    }

    UnitId get_current_unit_id() {
        if (!is_curent_cpu_enabled()) return 0;
        const Unit *u = get_current_unit();
        return (u && u->id) ? u->id : 0;
    }

    RealmId get_current_realm_id() {
        if (!is_curent_cpu_enabled()) return 0;
        const Unit *u = get_current_unit();
        return (u && u->rid) ? u->rid : 0;
    }

    capability_set get_current_capabilities() {
        Realm *r = get_current_realm();
        if (!r) return 0;

        return r->capabilities;
    }

    const char *get_current_cwd() {
        const Unit *u = get_current_unit();
        if (!u) return "/";
        const Realm *realm = u->parent;
        if (!realm) return "/";
        return realm->cwd_path;
    }

    bool set_current_cwd(const char *abs_path) {
        Unit *u = get_current_unit();
        if (!u) return false;
        Realm *realm = u->parent;
        if (!realm) return false;

        SpinlockGuard g(realm->lock);
        strncpy(realm->cwd_path, abs_path, sizeof(realm->cwd_path) - 1);
        realm->cwd_path[sizeof(realm->cwd_path) - 1] = '\0';
        return true;
    }

    bool is_initialized() {
        return global_scheduler.initialized;
    }

    u32 get_num_cpus() {
        return global_scheduler.num_cpus;
    }

    void wake_sleeping_units(u8 cpu_id) {
        cpu_scheduler::wake_sleeping_units(cpu_id);
    }

    void tick_cpu(u8 cpu_id, TrapFrame *frame) {
        cpu_scheduler::tick_cpu(cpu_id, frame);
    }

    bool on_user_fault(TrapFrame *frame, Signal sig, const char *fault_name) {
        if (!(frame->cs & 0x3)) {
            return false;
        }

        Unit *u = get_current_unit();

        if (Realm *realm = u->parent) {
            Log::print_ln("[%llu]  %s (core dumped)  %s", static_cast<u64>(realm->id), fault_name, realm->name);
        }

        signal_send(u, sig);
        signal_dispatch(u, frame);
        __builtin_unreachable();
    }

    void on_timer_tick(TrapFrame *frame) {
        if (!(frame->cs & 0x3)) return;

        Unit *u = get_current_unit();
        if (u && u->is_user && u->state == UnitState::Running) {
            signal_dispatch(u, frame);
        }
    }

    void on_syscall_exit(u64 ret) {
        Unit *u = get_current_unit();
        if (!u || !u->is_user) return;

        TrapFrame *trap = &u->context.current_trap_frame;
        trap->rax = ret;
        signal_dispatch(u, trap);
    }
}  // namespace kernel::scheduling