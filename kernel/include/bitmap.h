//
// Created by linus on 19.09.24.
//

#ifndef BITMAP_H
#include <stddef.h>
#include <stdint.h>
#define BITMAP_H

class Bitmap{
    public:
    size_t size;
    uint8_t* buffer;
    bool operator[](uint64_t index);
    bool set(uint64_t index, bool value);
    bool get(uint64_t index);

};

#endif //BITMAP_H