//
// Created by linus on 20.09.24.
//
#include "bitmap.h"

#include <vespera/types.h>

bool Bitmap::operator[](const u64 index) const {
    return get(index);
}

bool Bitmap::get(const u64 index) const {
    if (index > size * 8) return false;
    const u64 byte_index = index / 8;
    const u8 bit_index = index % 8;
    if (const u8 bit_indexer = 0b10000000 >> bit_index; (buffer[byte_index] & bit_indexer) > 0) {
        return true;
    }
    return false;
}

bool Bitmap::set(const u64 index, const bool value) const {
    if (index > size * 8) return false;
    const u64 byte_index = index / 8;
    const u8 bit_index = index % 8;
    const u8 bit_indexer = 0b10000000 >> bit_index;
    buffer[byte_index] &= ~bit_indexer;
    if (value) {
        buffer[byte_index] |= bit_indexer;
    }
    return true;
}
