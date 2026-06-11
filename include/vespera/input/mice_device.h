// mice_device.h
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

#ifndef VESPERAOS_INPUT_MICE_DEVICE_H
#define VESPERAOS_INPUT_MICE_DEVICE_H

#include <uapi/vespera/dev/mice.h>
#include <vespera/devices/char_device.h>
#include <vespera/devices/device_manager.h>
#include <vespera/input/input_event.h>
#include <vespera/sync/spinlock.h>
#include <vespera/sync/wait_queue.h>

namespace kernel::input {

    class MiceDevice final : public CharDevice {
       public:
        MiceDevice();
        ~MiceDevice() override;

        int open(CharFile**) override;
        int release(CharFile*) override;
        isize read(CharFile*, void* buf, usize count, usize offset) override;
        isize write(CharFile*, const void* buf, usize count) override;
        int poll(CharFile*) override;

        static void share_mouse_event(const InputEvent& ev);

       private:
        static void push_to_queue(MiceDevice* dev, const mice_event& ev);

        static constexpr usize BUFFER_SIZE = 128;
        mice_event buffer_[BUFFER_SIZE];
        volatile usize head_ = 0;
        volatile usize tail_ = 0;
        Spinlock lock_{};

        KernelDevice* devnode;

        WaitQueue wait_queue_;

        static MiceDevice* s_instance_;
    };

}  // namespace kernel::input

#endif  // VESPERAOS_INPUT_MICE_DEVICE_H