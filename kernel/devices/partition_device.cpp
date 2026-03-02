// partition_device.cpp
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

#include "partition_device.h"

#include "../types/handle.h"
#include "vespera_errno.h"

PartitionDevice::PartitionDevice(BlockDevice* parent, uint64_t start_lba, uint64_t length_lba)
    : parent(parent)
    , start_lba(start_lba)
    , length_lba(length_lba), sector_size(parent->get_sector_size()) {

    type = Type::Partition;
}

ssize_t PartitionDevice::read(const uint64_t lba, const uint32_t count, void* buf, size_t buf_size) {
    if (!parent) return false;
    if (lba + count > length_lba) return false;

    ssize_t ret = parent->read(start_lba + lba, count, buf, buf_size);

    return ret;
}

ssize_t PartitionDevice::write(const uint64_t lba, const uint32_t count, void* buf, size_t buf_size) {
    if (!parent || !buf) return -EINVAL;
    if (lba + count > length_lba) return -EINVAL;

    ssize_t ret = parent->write(start_lba + lba, count, buf, buf_size);

    return ret;
}