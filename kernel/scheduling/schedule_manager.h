//
// Created by Linus on 17.07.25.
//
#ifndef THREAD_MANAGER_H
#define THREAD_MANAGER_H

#include "../units/unit.h"
#include <cstdint>
#include "../../arch/x86_64/interrupts/interrupts_internal.h"

extern "C" void context_switch(void** old_sp, void* new_sp, uint32_t to_is_user, uint32_t from_is_user, int frame);

namespace kernel::scheduling::manager {

    void add_unit(Unit* thread);
    void remove_unit(Unit* thread);

    // Thread state management
    [[noreturn]] void terminate_current_unit();

    // Thread execution helpers
    extern "C" void unit_trampoline();
  //  extern "C" [[noreturn]] void idle_unit_func(void* arg);

    // Internal thread operations
    void switch_to_unit(Unit* from, Unit* to, interrupt_frame *frame);

    Unit *setup_idle_unit(uint8_t cpu_id);

} // namespace kernel::manager

#endif