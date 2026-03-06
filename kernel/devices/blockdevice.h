//
// Created by linus on 03.07.25.
//

#ifndef BLOCKDEVICE_H
#define BLOCKDEVICE_H

#include "../types/types.h"
#include <stddef.h>
#include <stdint.h>

class BlockDevice {
   public:
    enum class Type { Disk, Partition };

    Type type{Type::Disk};

    // bufferSize is not used to determine how much to read, but to assert, that the buffer is equal or greater than
    // sectorCount * sector_size
    virtual ssize_t read(uint64_t lba, size_t sector_count, void* buffer, size_t buffer_size) = 0;
    // bufferSize is not used to determine how much to write, but to assert, that the buffer is equal or greater than
    // sectorCount * sector_size
    virtual ssize_t write(uint64_t sector, size_t sector_count, void* buffer, size_t buffer_size) = 0;
    [[nodiscard]] virtual size_t get_size() const = 0;
    [[nodiscard]] virtual size_t get_sector_size() const = 0;
    virtual ~BlockDevice() = default;
};

#endif  // BLOCKDEVICE_H
