// intel_gem_object.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
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

#include "intel_gem_object.h"

#include <vespera/mm/memory.h>

namespace gpu::intel::core {
    bool GemObject::dec_ref() {
        if (__atomic_sub_fetch(&refcount, 1, __ATOMIC_ACQ_REL) != 0) {
            return false;
        }
        if (!is_userptr && !purged) {
            const usize page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
            kernel::memory::free_pages_phys(phys_addr, page_count);
        }
        delete this;
        return true;
    }
} // namespace gpu::intel::core