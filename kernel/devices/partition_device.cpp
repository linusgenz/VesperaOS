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

PartitionDevice::PartitionDevice(BlockDevice* parent, const u64 start_lba, const u64 length_lba)
    : parent_(parent)
    , start_lba_(start_lba)
    , length_lba_(length_lba), sector_size_(parent->get_sector_size()) {

    type = Type::Partition;
}

isize PartitionDevice::read(const u64 lba, const usize count, void* buf, const usize buf_size) {
    if (!parent_) return false;
    if (lba + count > length_lba_) return false;

    const isize ret = parent_->read(start_lba_ + lba, count, buf, buf_size);

    return ret;
}

isize PartitionDevice::write(const u64 lba, const usize count, void* buf, const usize buf_size) {
    if (!parent_ || !buf) return -EINVAL;
    if (lba + count > length_lba_) return -EINVAL;

    const isize ret = parent_->write(start_lba_ + lba, count, buf, buf_size);

    return ret;
}

bool PartitionDevice::trim(const TrimRange* ranges, const usize count) {
    if (!parent_ || !ranges || count == 0) return false;

    const auto translated = new TrimRange[count];

    for (usize i = 0; i < count; i++) {
        if (ranges[i].lba + ranges[i].sector_count > length_lba_) {
            delete[] translated;
            return false;
        }
        translated[i].lba          = start_lba_ + ranges[i].lba;
        translated[i].sector_count = ranges[i].sector_count;
    }

    const bool result = parent_->trim(translated, count);
    delete[] translated;
    return result;
}