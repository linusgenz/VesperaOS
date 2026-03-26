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


#include <vespera/types.h>

#include <vespera/devices/block.h>

class PartitionDevice final : public BlockDevice {
   public:
    PartitionDevice(BlockDevice* parent, u64 start_lba, u64 length_lba);
    ~PartitionDevice() override = default;

    isize read(u64 lba, usize count, void* buf, usize buf_size) override;
    isize write(u64 lba, usize count, const void* buf, usize buf_size) override;
    [[nodiscard]] usize get_sector_size() const override {
        return sector_size_;
    };
    [[nodiscard]] usize get_size() const override {
        return length_lba_ * get_sector_size();
    }

    [[nodiscard]] u64 get_start_lba() const {
        return start_lba_;
    }

    [[nodiscard]] u64 get_length_lba() const {
        return length_lba_;
    }

    [[nodiscard]] bool supports_trim() const override {
        return parent_ && parent_->supports_trim();
    }

    [[nodiscard]] BlockDevice* get_parent() const {
        return parent_;
    }

    bool trim(const TrimRange* ranges, usize count) override;

   private:
    BlockDevice* parent_;
    u64 start_lba_;
    u64 length_lba_;
    u64 sector_size_;
};

#endif  // VESPERAOS_PARTION_DEVICE_H
