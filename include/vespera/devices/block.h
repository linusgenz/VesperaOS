//
// Created by linus on 03.07.25.
//

#ifndef BLOCKDEVICE_H
#define BLOCKDEVICE_H

#include <vespera/types.h>

struct TrimRange {
    u64 lba;
    u32 sector_count;
};

class BlockDevice {
   public:
    enum class Type { Disk, Partition };

    Type type{Type::Disk};

    // bufferSize is not used to determine how much to read, but to assert, that the buffer is equal or greater than
    // sectorCount * sector_size
    virtual isize read(u64 lba, usize sector_count, void* buffer, usize buffer_size) = 0;
    // bufferSize is not used to determine how much to write, but to assert, that the buffer is equal or greater than
    // sectorCount * sector_size
    virtual isize write(u64 sector, usize sector_count, const void* buffer, usize buffer_size) = 0;
    [[nodiscard]] virtual usize get_size() const = 0;
    [[nodiscard]] virtual usize get_sector_size() const = 0;

    [[nodiscard]] virtual bool supports_trim() const {
        return false;
    }

    virtual bool trim(const TrimRange*, usize) {
        return false;
    }

    virtual ~BlockDevice() = default;
};

#endif  // BLOCKDEVICE_H
