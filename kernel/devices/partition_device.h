// partition_device.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 30.09.25.
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

#ifndef VESPERAOS_PARTION_DEVICE_H
#define VESPERAOS_PARTION_DEVICE_H

#include "blockdevice.h"
#include <cstdint>
#include <cstddef>

#include "../types/handle.h"

class PartitionDevice final : public BlockDevice
{
public:
    PartitionDevice(BlockDevice* parent, uint64_t start_lba, uint64_t length_lba);
    ~PartitionDevice() override = default;

    ssize_t read(uint64_t lba, uint32_t count, void* buf, size_t buf_size) override;
    ssize_t write(uint64_t lba, uint32_t count, void* buf, size_t buf_size) override;
    [[nodiscard]] uint32_t get_sector_size() const override { return sector_size; };
    [[nodiscard]] size_t get_size() const override { return length_lba * get_sector_size(); };

    [[nodiscard]] uint64_t get_start_lba() const { return start_lba; }

    [[nodiscard]] uint64_t get_length_lba() const { return length_lba; }

private:
    BlockDevice* parent;
    uint64_t start_lba;
    uint64_t length_lba;
    uint64_t sector_size;
};

#endif //VESPERAOS_PARTION_DEVICE_H
