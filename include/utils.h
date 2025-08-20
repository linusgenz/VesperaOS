//
// Created by linus on 09.11.2024.
//

#ifndef UTILS_H
#define UTILS_H

#include <cstdint>

inline uint64_t min(const uint64_t a, const uint64_t b) {
    return (a < b) ? a : b;
}

inline uint64_t max(const uint64_t a, const uint64_t b) {
    return (a > b) ? a : b;
}

#endif //UTILS_H
