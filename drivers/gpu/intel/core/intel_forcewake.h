// intel_forcewake.h
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
#ifndef VESPERAOS_INTEL_FORCEWAKE_H
#define VESPERAOS_INTEL_FORCEWAKE_H

#include <vespera/types.h>

namespace gpu::intel::core {

    /// ACK bit shared by every Gen9 ForceWake domain's ack register
    constexpr u32 FORCEWAKE_ACK_BIT = 0x1;

}  // namespace blt

#endif  // VESPERAOS_INTEL_FORCEWAKE_H
