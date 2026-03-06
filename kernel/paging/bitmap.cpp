//
// Created by linus on 20.09.24.
//
#include "bitmap.h"

#include <stdint.h>

bool Bitmap::operator[](const uint64_t index) const {
    return get(index);
}

bool Bitmap::get(const uint64_t index) const {
    if (index > size * 8) return false;
    const uint64_t byte_index = index / 8;
    const uint8_t bit_index = index % 8;
    if (const uint8_t bit_indexer = 0b10000000 >> bit_index; (buffer[byte_index] & bit_indexer) > 0) {
        return true;
    }
    return false;
}

bool Bitmap::set(const uint64_t index, const bool value) const {
    if (index > size * 8) return false;
    const uint64_t byte_index = index / 8;
    const uint8_t bit_index = index % 8;
    const uint8_t bit_indexer = 0b10000000 >> bit_index;
    buffer[byte_index] &= ~bit_indexer;
    if (value) {
        buffer[byte_index] |= bit_indexer;
    }
    return true;
}
