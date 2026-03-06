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

#include <vector.h>

#include "driver_lifecycle.h"
//#include <stdint.h>

struct VfsNode;
class CharDevice;
class BlockDevice;
class IRenderDriver;

enum class DeviceType : uint8_t {
    Block,
    Char,
    Controller,
    Bus,
    Logical,
    Gpu,
    Other,
};

enum class DeviceClass : uint8_t {
    Storage,
    Usb,
    Input,
    Net,
    Misc,
    Pseudo,
    Graphics,
    Unknown,
};

enum class ControllerType : uint8_t {
    None,
    Xhci,
    Ehci,
    Ohci,
    Uhci,
    Ahci,
    Nvme,
    VirtIo,
    Ps2,
    SmBus,
    IntelGpu,
    UefiGop,
    Other,
};

enum class BusType : uint8_t { VIRTUAL, None, Usb, Tty, I2C, Spi, Ps2, Pci };

struct KernelDevice {
    uint32_t id{};
    const char* name{};
    DeviceType type{DeviceType::Other};
    DeviceClass dev_class{DeviceClass::Unknown};
    ControllerType controller;
    BusType bus_type{BusType::None};

    VfsNode* vfs_node_parent{};

    KernelDevice* parent{nullptr};
    Vector<KernelDevice*> children;

    BlockDevice* block{nullptr};
    CharDevice* chardev{nullptr};

    IDriverLifecycle* lifecycle{nullptr};

    uint32_t next_nvme_index = 0;  // for nvme<N> controller
    Vector<bool> nvme_device_used;

    void* driver_data{nullptr};  // optional pointer for drivers
};

class DeviceManager {
   public:
    static void init();
    static char* generate_sd_device_name(char* buffer, size_t buffer_size);

    static char* generate_nv_me_device_name(
        const KernelDevice* controller, char* buffer, size_t buffer_size, uint32_t namespace_id
    );
    static size_t find_and_register_partitions(KernelDevice* physical_kd);
    static bool alloc_unique_device_name(const char* base, char* out_buffer, size_t out_buffer_size);

    // legacy
    static Vector<BlockDevice*> get_devices();
    static uint32_t get_device_count();

    static KernelDevice* register_block_device(
        BlockDevice* dev, const char* name, DeviceClass dev_class = DeviceClass::Storage,
        BusType bus = BusType::None, ControllerType controller = ControllerType::Other,
        KernelDevice* parent = nullptr
    );

    static KernelDevice* register_char_device(
        CharDevice* dev, const char* name, DeviceClass dev_class, BusType bus,
        ControllerType controller = ControllerType::Other, KernelDevice* parent = nullptr
    );

    static KernelDevice* register_controller(
        const char* name, DeviceClass dev_class, BusType bus, ControllerType controller = ControllerType::Other,
        KernelDevice* parent = nullptr, CharDevice* dev = nullptr, IDriverLifecycle* lifecycle = nullptr
    );
    static KernelDevice* register_gpu_device(
        IRenderDriver* driver, const char* name, DeviceClass dev_class, BusType bus, ControllerType controller,
        KernelDevice* parent
    );
    static void unregister_device(KernelDevice* kd);

    static Vector<KernelDevice*> get_all_devices();
    static KernelDevice* find_by_id(uint32_t id);
    static uint32_t get_kernel_device_count();

    static void shutdown_all();
    static void suspend_all();
    static void resume_all();

   private:
    static Vector<BlockDevice*>* devices_;
    static Vector<KernelDevice*>* all_devices_;
    static uint32_t next_id_;
    static Spinlock lock_;

    static void release_block_letter(char c);
    static char get_next_free_block_letter();
};

#endif  // DEVICE_MANAGER_H
