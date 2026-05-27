// mouse.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 27.05.26.
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

#ifndef VESPERAOS_DRIVERS_PS2_MOUSE_H
#define VESPERAOS_DRIVERS_PS2_MOUSE_H

#include <vespera/types.h>

namespace ps2::mouse {

    class Ps2Mouse {
       public:
        Ps2Mouse() = delete;

        static void init();
        static void handle_byte(u8 data);
        static point_t get_position();

       private:
        static point_t position_;
        static point_t position_old_;
        static u8 packet_[4];
        static u8 cycle_;
        static bool packet_ready_;
        static bool first_byte_skipped_;

        static void process_packet();
    };

}  // namespace ps2::mouse

#endif  // VESPERAOS_DRIVERS_PS2_MOUSE_H
