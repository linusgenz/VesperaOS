//
// Created by linus on 03.07.25.
//

#ifndef BLOCKDEVICE_H
#define BLOCKDEVICE_H

#include <cstdint>
#include <cstddef>
#include "../types/types.h"

class BlockDevice {
public:
    enum class Type {
        Disk,
        Partition
    };

    Type type{Type::Disk};

    virtual ssize_t read(uint64_t lba, uint32_t sectorCount, void* buffer) = 0;
    virtual ssize_t write(uint64_t sector, uint32_t sectorCount, void *buffer) = 0;
    [[nodiscard]] virtual size_t get_size() const = 0;
    [[nodiscard]] virtual uint32_t get_sector_size() const { return 512; } // Default 512 Bytes
    virtual ~BlockDevice() = default;
};

#endif //BLOCKDEVICE_H
