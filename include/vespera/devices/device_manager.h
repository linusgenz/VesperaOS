// device_manager.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 01.08.25.
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

#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H
#include "device_descriptor.h"
#include "kernel_device.h"
#include "vespera/sync/spinlock.h"

class DeviceManager {
   public:
    static void init();

    static KernelDevice* register_device(const DeviceDescriptor& desc);

    static void unregister_device(KernelDevice* kd);

    static char* generate_sd_device_name(char* buffer, usize buffer_size);
    static char* generate_nvme_device_name(
        const KernelDevice* controller, char* buffer, usize buffer_size, u32 namespace_id
    );
    static usize find_and_register_partitions(KernelDevice* physical_kd);
    static bool alloc_unique_device_name(const char* base, char* out_buffer, usize out_buffer_size);

    static Vector<KernelDevice*> get_all_devices();
    static KernelDevice* find_by_id(u32 id);
    static u32 get_kernel_device_count();

    static usize get_device_count();

    static void shutdown_all();
    static void suspend_all();
    static void resume_all();

    template<typename Predicate>
    static Vector<KernelDevice*> query(Predicate pred) {
        Vector<KernelDevice*> result;
        SpinlockGuard guard(lock_);
        if (!all_devices_) return result;
        for (auto* kd : *all_devices_) {
            if (kd && pred(kd)) result.push_back(kd);
        }
        return result;
    }

   private:
    static Vector<BlockDevice*>* devices_;
    static Vector<KernelDevice*>* all_devices_;
    static u32 next_id_;
    static Spinlock lock_;

    static void release_block_letter(char c);
    static char get_next_free_block_letter();
};

#endif  // DEVICE_MANAGER_H
