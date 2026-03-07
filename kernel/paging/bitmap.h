//
// Created by linus on 19.09.24.
//

#ifndef BITMAP_H
#define BITMAP_H


#include <vespera/types.h>

class Bitmap {
   public:
    usize size;
    u8* buffer;
    bool operator[](u64 index) const;
    [[nodiscard]] bool set(u64 index, bool value) const;
    [[nodiscard]] bool get(u64 index) const;
};

#endif  // BITMAP_H