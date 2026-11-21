// vm.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 13.05.26.
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

#ifndef VESPERAOS_VESPERA_MM_VM_H
#define VESPERAOS_VESPERA_MM_VM_H

#include <vespera/types.h>

class Unit;

namespace kernel::vm {

    /**
     * @brief Maps anonymous pages into the current unit's address space.
     *
     * Allocates physical frames, maps them into the realm's page table, and
     * records a VmArea on the unit. Only MAP_ANONYMOUS is currently supported.
     *
     * @param u       The unit to map into. Must be a user unit.
     * @param addr    Hint address or 0 to let the kernel choose.
     * @param length  Mapping length in bytes; rounded up to PAGE_SIZE.
     * @param prot    PROT_* flags.
     * @param flags   MAP_* flags.
     * @param handle  File handle for file-backed mappings (currently unused).
     * @param offset  File offset for file-backed mappings (currently unused).
     *
     * @return Mapped virtual address on success, negative errno on failure.
     */
    [[nodiscard]] i64 mmap(Unit* u, uptr addr, usize length, u64 prot, u64 flags, u64 handle, u64 offset);

    /**
     * @brief Unmaps a range from the current unit's address space.
     *
     * @return 0 on success, negative errno on failure.
     */
    [[nodiscard]] i64 munmap(Unit* u, uptr addr, usize length);

    /**
     * @brief Adjusts the program break of the given unit.
     *
     * If @p addr is 0, returns the current break. Grows or shrinks the heap
     * by mapping or unmapping pages and updating the heap VmArea.
     *
     * @return New break address on success, negative errno on failure.
     */
    [[nodiscard]] i64 brk(Unit* u, uptr addr);

    i64 mprotect(Unit* u, const uptr addr, usize length, const u64 prot);

} // namespace kernel::vm

#endif // VESPERAOS_VESPERA_MM_VM_H