// fault.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 12.05.26.
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

#ifndef VESPERAOS_VESPERA_SCHED_SCHED_HOOKS_H
#define VESPERAOS_VESPERA_SCHED_SCHED_HOOKS_H

struct TrapFrame;
enum class Signal : u32;

namespace kernel::scheduling {

  /**
   * @brief Fault hook — called by arch fault handlers on any user-mode fault.
   *
   * Logs realm identity, sends @p sig to the current unit, and dispatches it.
   * Never returns when the faulting context was user-mode (CS & 0x3).
   *
   * @return false if the fault occurred in kernel mode; caller must handle
   *         the kernel panic path.
   */
  [[nodiscard]] bool on_user_fault(TrapFrame* frame,
                                   Signal sig,
                                   const char* fault_name);

  /**
   * @brief Timer hook — called on every APIC timer interrupt.
   *
   * Dispatches pending signals to the current unit if it is a running
   * user-mode unit interrupted mid-execution.
   */
  void on_timer_tick(TrapFrame* frame);

  /**
 * @brief Called at syscall exit to dispatch pending signals.
 *
 * Updates the TrapFrame return value and dispatches any pending signals
 * to the current unit if it is a user-mode unit.
 *
 * @param ret  The syscall return value to write into rax.
 */
  void on_syscall_exit(u64 ret);
}  // namespace kernel::scheduling

#endif  // VESPERAOS_VESPERA_SCHED_SCHED_HOOKS_H