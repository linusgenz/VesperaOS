// intel_engine_types.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 06.08.26.
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

#ifndef VESPERAOS_INTEL_ENGINE_TYPES_H
#define VESPERAOS_INTEL_ENGINE_TYPES_H

enum class EngineType : u8 {
    RCS,   ///< Render Command Streamer
    BCS,   ///< Blitter Command Streamer
    VCS0,  ///< Video Command Streamer 0 (BSD)
    VCS1,  ///< Video Command Streamer 1
    VECS,  ///< Video Enhancement Command Streamer
};

[[nodiscard]] constexpr const char* engine_type_to_string(EngineType type) {
    switch (type) {
        case EngineType::RCS:  return "RCS";
        case EngineType::BCS:  return "BCS";
        case EngineType::VCS0: return "VCS0";
        case EngineType::VCS1: return "VCS1";
        case EngineType::VECS: return "VECS";
    }
    return "UNKNOWN";
}

#endif // VESPERAOS_INTEL_ENGINE_TYPES_H