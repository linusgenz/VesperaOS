/**
 * @file sort.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 07.01.26.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef VESPERAOS_SORT_H
#define VESPERAOS_SORT_H

#include <cstddef>
#include <utility>
#include <iterator.h>

namespace klib
{
    constexpr size_t INSERTION_SORT_THRESHOLD = 16;

constexpr size_t log2_floor(size_t n) {
    size_t log = 0;
    while (n >>= 1) ++log;
    return log;
}

template<typename RandomIt, typename Compare>
void insertion_sort(RandomIt first, RandomIt last, Compare comp) {
    if (first == last) return;

    for (RandomIt i = first + 1; i != last; ++i) {
        auto val = std::move(*i);
        RandomIt j = i;

        // Optimierung: unrolled comparison
        while (j != first && comp(val, *(j - 1))) {
            *j = std::move(*(j - 1));
            --j;
        }
        *j = std::move(val);
    }
}

// Median-of-Three Pivot-Select
template<typename RandomIt, typename Compare>
RandomIt median_of_three(RandomIt a, RandomIt b, RandomIt c, Compare comp) {
    if (comp(*a, *b)) {
        if (comp(*b, *c))
            return b;
        else if (comp(*a, *c))
            return c;
        else
            return a;
    } else {
        if (comp(*a, *c))
            return a;
        else if (comp(*b, *c))
            return c;
        else
            return b;
    }
}

// Partition with Hoare Scheme
template<typename RandomIt, typename Compare>
RandomIt partition(RandomIt first, RandomIt last, Compare comp) {
    RandomIt mid = first + (last - first) / 2;
    RandomIt pivot_it = median_of_three(first, mid, last - 1, comp);

    auto pivot = *pivot_it;
    std::swap(*pivot_it, *(last - 1));

    RandomIt i = first - 1;
    RandomIt j = last;

    while (true) {
        do { ++i; } while (comp(*i, pivot));
        do { --j; } while (comp(pivot, *j));

        if (i >= j) return j + 1;
        std::swap(*i, *j);
    }
}

// Heapsort als Fallback bei zu tiefer Rekursion
template<typename RandomIt, typename Compare>
void heapsort(RandomIt first, RandomIt last, Compare comp) {
    using diff_t = iterator_traits<RandomIt>::difference_type;
    diff_t n = last - first;

    // Build heap
    for (diff_t i = n / 2 - 1; i >= 0; --i) {
        diff_t parent = i;
        auto val = std::move(first[parent]);

        while (true) {
            diff_t child = 2 * parent + 1;
            if (child >= n) break;

            // Finde größeres Kind
            if (child + 1 < n && comp(first[child], first[child + 1]))
                ++child;

            if (!comp(val, first[child])) break;

            first[parent] = std::move(first[child]);
            parent = child;
        }
        first[parent] = std::move(val);
    }

    // Extract elements
    for (diff_t i = n - 1; i > 0; --i) {
        std::swap(first[0], first[i]);

        diff_t parent = 0;
        auto val = std::move(first[parent]);

        while (true) {
            diff_t child = 2 * parent + 1;
            if (child >= i) break;

            if (child + 1 < i && comp(first[child], first[child + 1]))
                ++child;

            if (!comp(val, first[child])) break;

            first[parent] = std::move(first[child]);
            parent = child;
        }
        first[parent] = std::move(val);
    }
}

template<typename RandomIt, typename Compare>
void introsort_impl(RandomIt first, RandomIt last, size_t depth_limit, Compare comp) {
    while (last - first > INSERTION_SORT_THRESHOLD) {
        if (depth_limit == 0) {
            heapsort(first, last, comp);
            return;
        }
        --depth_limit;

        RandomIt cut = partition(first, last, comp);

        // Tail call optimization, reduces stack usage
        if (cut - first < last - cut) {
            introsort_impl(first, cut, depth_limit, comp);
            first = cut;
        } else {
            introsort_impl(cut, last, depth_limit, comp);
            last = cut;
        }
    }
}

template<typename RandomIt, typename Compare>
void sort(RandomIt first, RandomIt last, Compare comp) {
    if (first == last) return;

    size_t n = last - first;
    size_t depth_limit = 2 * log2_floor(n);

    introsort_impl(first, last, depth_limit, comp);
    insertion_sort(first, last, comp);
}

template<typename RandomIt>
void sort(RandomIt first, RandomIt last) {
    sort(first, last, [](const auto& a, const auto& b) { return a < b; });
}
}
#endif //VESPERAOS_SORT_H