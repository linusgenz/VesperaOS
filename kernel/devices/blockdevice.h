//
// Created by linus on 03.07.25.
//

#ifndef BLOCKDEVICE_H
#define BLOCKDEVICE_H

#include "../types/types.h"
#include "kernel/memory.h"
#include "log.h"
#include <cstddef>
#include <cstdint>

class BlockDevice {
   public:
    enum class Type { Disk, Partition };

    Type type{Type::Disk};

    // bufferSize is not used to determine how much to read, but to assert, that the buffer is equal or greater than
    // sectorCount * sector_size
    virtual ssize_t read(uint64_t lba, uint32_t sectorCount, void* buffer, size_t bufferSize) = 0;
    // bufferSize is not used to determine how much to write, but to assert, that the buffer is equal or greater than
    // sectorCount * sector_size
    virtual ssize_t write(uint64_t sector, uint32_t sectorCount, void* buffer, size_t bufferSize) = 0;
    [[nodiscard]] virtual size_t get_size() const = 0;
    [[nodiscard]] virtual uint32_t get_sector_size() const = 0;
    virtual ~BlockDevice() = default;
};

#endif  // BLOCKDEVICE_H
