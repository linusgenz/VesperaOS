//
// Created by Linus on 17.07.25.
//
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <vespera/types.h>

struct TrapFrame;
class Unit;
class Realm;
typedef u64 capability_set;

namespace kernel::scheduling::cpu_scheduler {
    struct CpuScheduler;
}

namespace kernel::scheduling {

    // Global scheduler operations
    void init(u32 num_cpus);
    void yield();
    void yield_cpu(u8 cpu_id, TrapFrame* frame);
    //   void tick();

    // Global thread management
    void add_unit(Unit* unit);
    void remove_unit(Unit* unit);
    void add_blocked_unit(Unit* unit, u8 cpu_id);
    void remove_blocked_unit(Unit* unit);

    // CPU management
    void enable_on_cpu(u8 cpu_id);
    void disable_on_cpu(u8 cpu_id);

    bool is_curent_cpu_enabled();

    /**
     * @brief Sets the FS base of the current unit and applies it to MSR_FS_BASE.
     * @return false if no unit is active.
     */
    bool set_fs_base(u64 addr);

    /**
     * @brief Returns the FS base of the current unit via @p out.
     * @return false if no unit is active or @p out is null.
     */
    bool get_fs_base(u64* out);

    /**
     * @brief Returns whether the currently running unit on this CPU is the idle unit.
     *
     * Safe to call from interrupt context before the scheduler is fully initialized;
     * returns true if no unit is running.
     */
    [[nodiscard]] bool is_current_unit_idle();

    /**
     * @brief Returns the accumulated CPU time of all units in the given realm.
     *
     * Includes the currently running slice of the active unit if applicable.
     *
     * @param realm_id  Target realm ID.
     * @return Total nanoseconds of CPU time, or 0 if the realm is not found.
     */
    [[nodiscard]] u64 get_realm_cpu_time_ns(RealmId realm_id);

    // Query functions
    Unit* get_current_unit();

    /**
     * @brief Returns the Realm of the currently running unit, or nullptr
     *        if no unit is active or the unit has no parent realm.
     *
     * Replaces the get_current_unit() → unit->parent pattern in syscall handlers.
     */
    Realm* get_current_realm();

    /**
     * @brief Returns the cwd of the current realm, or "/" if no realm
     *        is active (early boot, idle, kernel unit).
     */
    const char* get_current_cwd();

    /**
     * @brief Sets the cwd of the current realm.
     * @return false if no realm is active.
     */
    bool set_current_cwd(const char* abs_path);

    /**
     * @brief Returns the ID of the currently running unit, or 1 if no unit
     *        is active (early boot, idle, or CPU not yet enabled).
     *
     * Intended for external subsystems (ACPI, timekeeping, …) that need
     * a thread identity but must not depend on Unit internals.
     */
    UnitId get_current_unit_id();

    /**
     * @brief Returns the RealmId of the currently running unit's realm.
     *
     * Returns 0 if no realm is active.
     */
    [[nodiscard]] RealmId get_current_realm_id();

    capability_set get_current_capabilities();

    bool is_initialized();
    u32 get_num_cpus();

    void wake_sleeping_units(u8 cpu_id);

    void tick_cpu(u8 cpu_id, TrapFrame* frame);
}  // namespace kernel::scheduling

#endif  // SCHEDULER_H