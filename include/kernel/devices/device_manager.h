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
#include <cstdint>

struct VfsNode;
class CharDevice;
class BlockDevice;
class IRenderDriver;

enum class DeviceType : uint8_t
{
    Block,
    Char,
    Controller,
    Bus,
    Logical,
    Gpu,
    Other,
};

enum class DeviceClass : uint8_t
{
    Storage,
    Usb,
    Input,
    Net,
    Misc,
    Pseudo,
    Graphics,
    Unknown,
};

enum class ControllerType : uint8_t
{
    None,
    XHCI,
    EHCI,
    OHCI,
    UHCI,
    AHCI,
    NVMe,
    VirtIO,
    PS2,
    SMBus,
    IntelGPU,
    UefiGOP,
    Other,
};

enum class BusType : uint8_t
{
    VIRTUAL,
    BUS_NONE,
    BUS_USB,
    BUS_TTY,
    BUS_I2C,
    BUS_SPI,
    BUS_PS2,
    BUS_PCI,
    BUS_MAX
};

struct KernelDevice
{
    uint32_t id{};
    const char* name{};
    DeviceType type{DeviceType::Other};
    DeviceClass dev_class{DeviceClass::Unknown};
    ControllerType controller;
    BusType bus_type{BusType::BUS_NONE};

    VfsNode* vfs_node_parent{};

    KernelDevice* parent{nullptr};
    Vector<KernelDevice*> children{};

    BlockDevice* block{nullptr};
    CharDevice* chardev{nullptr};

    uint32_t next_nvme_index = 0; // for nvme<N> controller
    Vector<bool> nvme_device_used;

    void* driver_data{nullptr}; // optional pointer for drivers
};

class DeviceManager
{
public:
    static void init();
    static char* GenerateSDDeviceName(char* buffer, size_t buffer_size);

    static char* GenerateNVMeDeviceName(const KernelDevice* controller, char* buffer, size_t buffer_size, uint32_t namespaceId);
    static size_t FindAndRegisterPartitions(KernelDevice* physical_kd);
    static bool AllocUniqueDeviceName(const char* base, char* outBuffer, size_t outBufferSize);

    // legacy
    static Vector<BlockDevice*> GetDevices();
    static uint32_t GetDeviceCount();

    static KernelDevice* RegisterBlockDevice(BlockDevice* dev,
                                             const char* name,
                                             DeviceClass dev_class = DeviceClass::Storage,
                                             BusType bus = BusType::BUS_NONE,
                                             ControllerType controller = ControllerType::Other,
                                             KernelDevice* parent = nullptr);

    static KernelDevice* RegisterCharDevice(CharDevice* dev,
                                            const char* name,
                                            DeviceClass dev_class,
                                            BusType bus,
                                            ControllerType controller = ControllerType::Other,
                                            KernelDevice* parent = nullptr);

    static KernelDevice* RegisterController(
        const char* name,
        DeviceClass dev_class,
        BusType bus,
        ControllerType controller = ControllerType::Other,
        KernelDevice* parent = nullptr, ::CharDevice* dev = nullptr);
    static KernelDevice* RegisterGpuDevice(IRenderDriver* driver, const char* name, DeviceClass dev_class, BusType bus,
                                    ControllerType controller, KernelDevice* parent);
    static void UnregisterDevice(KernelDevice* kd);

    static Vector<KernelDevice*> GetAllDevices();
    static KernelDevice* FindById(uint32_t id);
    static uint32_t GetKernelDeviceCount();

private:
    static Vector<BlockDevice*>* devices;
    static Vector<KernelDevice*>* all_devices;
    static uint32_t next_id;
    static spinlock_t lock;

    static void ReleaseBlockLetter(char c);
    static char GetNextFreeBlockLetter();
};

#endif //DEVICE_MANAGER_H
