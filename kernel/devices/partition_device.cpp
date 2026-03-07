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

#include "vespera_errno.h"

PartitionDevice::PartitionDevice(BlockDevice* parent, uint64_t start_lba, uint64_t length_lba)
    : parent_(parent)
    , start_lba_(start_lba)
    , length_lba_(length_lba), sector_size_(parent->get_sector_size()) {

    type = Type::Partition;
}

ssize_t PartitionDevice::read(const uint64_t lba, const size_t count, void* buf, size_t buf_size) {
    if (!parent_) return false;
    if (lba + count > length_lba_) return false;

    ssize_t ret = parent_->read(start_lba_ + lba, count, buf, buf_size);

    return ret;
}

ssize_t PartitionDevice::write(const uint64_t lba, const size_t count, void* buf, size_t buf_size) {
    if (!parent_ || !buf) return -EINVAL;
    if (lba + count > length_lba_) return -EINVAL;

    ssize_t ret = parent_->write(start_lba_ + lba, count, buf, buf_size);

    return ret;
}