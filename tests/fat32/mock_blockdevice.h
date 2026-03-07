// mock_blockdevice.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 02.03.26.
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

#ifndef MOCK_BLOCKDEVICE_H
#define MOCK_BLOCKDEVICE_H

#include <stdint.h>
#include <cstring>
#include <vector>
#include <cassert>

// ===== MOCK IMPLEMENTIERUNG =====

class MockBlockDevice : public BlockDevice {
public:
    static constexpr size_t SECTOR_SIZE = 512;

    explicit MockBlockDevice(size_t sectorCount)
        : storage(sectorCount * SECTOR_SIZE, 0)
    {
        type = Type::Disk;
    }

    ssize_t read(uint64_t lba,
                 size_t sectorCount,
                 void* buffer,
                 size_t bufferSize) override
    {
        size_t len = sectorCount * SECTOR_SIZE;
        size_t offset = lba * SECTOR_SIZE;

        if (bufferSize < len) return -1;
        if (offset + len > storage.size()) return -1;

        std::memcpy(buffer, storage.data() + offset, len);
        return len;
    }

    ssize_t write(uint64_t lba,
                  size_t sectorCount,
                  void* buffer,
                  size_t bufferSize) override
    {
        size_t len = sectorCount * SECTOR_SIZE;
        size_t offset = lba * SECTOR_SIZE;

        if (bufferSize < len) return -1;
        if (offset + len > storage.size()) return -1;

        std::memcpy(storage.data() + offset, buffer, len);
        return len;
    }

    size_t get_size() const override {
        return storage.size();
    }

    size_t get_sector_size() const override {
        return SECTOR_SIZE;
    }

    // Test helper
    uint8_t* raw() {
        return storage.data();
    }

private:
    std::vector<uint8_t> storage;
};

#endif  // MOCK_BLOCKDEVICE_H
