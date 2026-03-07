//
// Created by linus on 09.11.2024.
//

#ifndef UTILS_H
#define UTILS_H

#include <vespera/types.h>

inline u64 min(const u64 a, const u64 b) {
    return (a < b) ? a : b;
}

inline u64 max(const u64 a, const u64 b) {
    return (a > b) ? a : b;
}

#endif //UTILS_H
