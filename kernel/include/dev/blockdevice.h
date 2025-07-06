//
// Created by linus on 03.07.25.
//

#ifndef BLOCKDEVICE_H
#define BLOCKDEVICE_H


#include <stdint.h>

class BlockDevice {
public:
    virtual bool read(uint64_t lba, uint32_t sectorCount, void* buffer) = 0;
    virtual bool write(uint64_t sector, uint32_t sectorCount, void *buffer) const = 0;
 //   virtual bool write(uint64_t lba, uint32_t sectorCount, const void* buffer) { return false; } // optional
    virtual uint32_t get_sector_size() { return 512; } // Default 512 Bytes
    virtual ~BlockDevice() = default;
};

#endif //BLOCKDEVICE_H
