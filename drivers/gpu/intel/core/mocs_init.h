// mocs_init.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 04.09.26.
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

#ifndef VESPERAOS_MOCS_INIT_H
#define VESPERAOS_MOCS_INIT_H

#include <vespera/types.h>

namespace gpu::intel::core {

    /// Programs all 64 GFX_MOCS registers (0xC800-0xC8FC) and the 32
    /// LNCFCMOCS registers (0xB020-0xB09C) to well-defined values.
    void init_mocs_table(volatile u8* mmio_base);

}  // namespace gpu::intel::core

#endif  // VESPERAOS_MOCS_INIT_H
