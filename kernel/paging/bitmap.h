//
// Created by linus on 19.09.24.
//

#ifndef BITMAP_H
#include <cstddef>
#include <cstdint>
#define BITMAP_H

class Bitmap{
    public:
    size_t size;
    uint8_t* buffer;
    bool operator[](uint64_t index) const;
    [[nodiscard]] bool set(uint64_t index, bool value) const;
    [[nodiscard]] bool get(uint64_t index) const;

};

#endif //BITMAP_H