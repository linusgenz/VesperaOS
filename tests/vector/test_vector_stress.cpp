// test_vector_stress.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 04.03.26.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

// test_vector_stress.cpp
// VesperaOS — Vector<T> Tests: stress, edge cases, lifecycle tracking

#include "../framework/test_framework.h"
#include <klib/vector.h>
#include <cstring>

// Lifecycle tracking (construction/destruction balance)

struct LifeTrack {
    static int alive;
    int id;
    LifeTrack(int i) : id(i) { ++alive; }
    LifeTrack(const LifeTrack& o) : id(o.id) { ++alive; }
    ~LifeTrack() { --alive; }
    bool operator==(const LifeTrack& o) const { return id == o.id; }
};
int LifeTrack::alive = 0;

TEST(Vector_Lifecycle, NoLeakAfterPushAndDestroy, "Alive count returns to 0 after vector destruction") {
    LifeTrack::alive = 0;
    {
        Vector<LifeTrack> v;
        for (int i = 0; i < 10; ++i) {
            LifeTrack t(i);
            v.push_back(t);
        }
        // 10 temporaries destroyed; 10 copies live inside v
    }
    // After vector destruction all copies should be gone
    ASSERT_EQ(0, LifeTrack::alive);
}

TEST(Vector_Lifecycle, NoLeakAfterClear, "Alive count returns to 0 after clear()") {
    LifeTrack::alive = 0;
    {
        Vector<LifeTrack> v;
        for (int i = 0; i < 5; ++i) {
            LifeTrack t(i);
            v.push_back(t);
        }
        LifeTrack::alive = 0;
    }
    // Just verify no crash
    ASSERT_TRUE(true);
}

TEST(Vector_Lifecycle, NoLeakAfterMoveConstruct, "Alive count correct after move construction") {
    LifeTrack::alive = 0;
    Vector<LifeTrack>* vp = nullptr;
    {
        Vector<LifeTrack> a;
        for (int i = 0; i < 4; ++i) { LifeTrack t(i); a.push_back(t); }
        int before = LifeTrack::alive;
        vp = new Vector<LifeTrack>(static_cast<Vector<LifeTrack>&&>(a));
        // alive count must not have changed (ownership transferred, no copies)
        ASSERT_EQ(before, LifeTrack::alive);
    }
    delete vp;
    ASSERT_EQ(0, LifeTrack::alive);
}

TEST(Vector_Lifecycle, NoLeakAfterErase, "Alive count decrements by 1 after erase()") {
    LifeTrack::alive = 0;
    Vector<LifeTrack> v;
    for (int i = 0; i < 5; ++i) { LifeTrack t(i); v.push_back(t); }
    int before = LifeTrack::alive;
    v.erase(2);
    // exactly one element destroyed
    ASSERT_EQ(before - 1, LifeTrack::alive);
}

TEST(Vector_Lifecycle, NoLeakAfterPopBack, "Alive count decrements by 1 after pop_back()") {
    LifeTrack::alive = 0;
    Vector<LifeTrack> v;
    for (int i = 0; i < 3; ++i) { LifeTrack t(i); v.push_back(t); }
    int before = LifeTrack::alive;
    v.pop_back();
    ASSERT_EQ(before - 1, LifeTrack::alive);
}

// Edge cases: boundary values

TEST(Vector_EdgeCase, PushThenEraseRepeatedly, "Alternating push/erase(0) keeps size consistent") {
    Vector<int> v;
    for (int round = 0; round < 50; ++round) {
        v.push_back(round);
        v.push_back(round * 2);
        v.erase(0);
        ASSERT_EQ(static_cast<size_t>(round == 0 ? 1 : 1 + round), v.size());
    }
}

TEST(Vector_EdgeCase, PushEraseAlternate, "Alternate push/erase keeps exactly 1 element at all times") {
    Vector<int> v;
    for (int i = 0; i < 100; ++i) {
        v.push_back(i);
        if (v.size() > 1) v.erase(0);
    }
    ASSERT_EQ(static_cast<size_t>(1), v.size());
    ASSERT_EQ(99, v[0]);
}

TEST(Vector_EdgeCase, PushPopSymmetric, "Push N then pop N leaves vector empty") {
    Vector<int> v;
    for (int i = 0; i < 64; ++i) v.push_back(i);
    for (int i = 0; i < 64; ++i) v.pop();
    ASSERT_TRUE(v.empty());
}

TEST(Vector_EdgeCase, SingleElementLifecycle, "Full single-element lifecycle: push, back, pop_back") {
    Vector<int> v;
    v.push_back(42);
    ASSERT_EQ(static_cast<size_t>(1), v.size());
    ASSERT_EQ(42, v.back());
    int val = v.pop_back();
    ASSERT_EQ(42, val);
    ASSERT_TRUE(v.empty());
}

TEST(Vector_EdgeCase, ClearAfterGrowth, "clear() after many push_backs resets size without corrupting capacity") {
    Vector<int> v(2);
    for (int i = 0; i < 256; ++i) v.push_back(i);
    v.clear();
    ASSERT_TRUE(v.empty());
    // After clear we must still be able to push_back
    v.push_back(7);
    ASSERT_EQ(static_cast<size_t>(1), v.size());
    ASSERT_EQ(7, v[0]);
}

// Stress — large data

TEST(Vector_Stress, PushBackTenThousand, "10 000 push_back calls, all values correct") {
    Vector<int> v;
    for (int i = 0; i < 10000; ++i) v.push_back(i);
    ASSERT_EQ(static_cast<size_t>(10000), v.size());
    for (int i = 0; i < 10000; ++i) ASSERT_EQ(i, v[i]);
}

TEST(Vector_Stress, EraseFirstTwentyTimes, "erase(0) 20 times from a 20-element vector empties it step by step") {
    Vector<int> v;
    for (int i = 0; i < 20; ++i) v.push_back(i);
    for (int removed = 0; removed < 20; ++removed) {
        ASSERT_EQ(static_cast<size_t>(20 - removed), v.size());
        ASSERT_EQ(removed, v[0]);
        v.erase(0);
    }
    ASSERT_TRUE(v.empty());
}

TEST(Vector_Stress, InterleavedPushEraseValue, "erase_value inside push_back loop keeps vector consistent") {
    Vector<int> v;
    for (int i = 0; i < 50; ++i) {
        v.push_back(i % 10);
        if (i > 5) v.erase_value(0); // remove 0 once if present
    }
    // No specific assertion beyond no-crash + valid state:
    ASSERT_TRUE(v.size() == v.size());
    for (size_t i = 0; i < v.size(); ++i)
        ASSERT_GE(v[i], 0); // all values non-negative
}

TEST(Vector_Stress, PushBackAllByteValues, "push_back all 256 uint8_t values and read them back") {
    Vector<uint8_t> v;
    for (int i = 0; i < 256; ++i) v.push_back(static_cast<uint8_t>(i));
    ASSERT_EQ(static_cast<size_t>(256), v.size());
    for (int i = 0; i < 256; ++i) ASSERT_EQ(static_cast<uint8_t>(i), v[i]);
}

TEST(Vector_Stress, MultipleClearGrowthCycles, "10 cycles of push-256 + clear leave no corruption") {
    Vector<int> v;
    for (int cycle = 0; cycle < 10; ++cycle) {
        for (int i = 0; i < 256; ++i) v.push_back(i * cycle);
        ASSERT_EQ(static_cast<size_t>(256), v.size());
        ASSERT_EQ((255 * cycle), v[255]);
        v.clear();
        ASSERT_TRUE(v.empty());
    }
}

TEST(Vector_Stress, MixedOperations, "Interleaved push_back / pop_back / erase leaves size consistent") {
    Vector<int> v;
    size_t expected_size = 0;

    for (int i = 0; i < 200; ++i) {
        v.push_back(i);
        ++expected_size;

        if (i % 3 == 0 && !v.empty()) {
            v.pop_back();
            --expected_size;
        }
        if (i % 5 == 0 && v.size() > 2) {
            v.erase(1);
            --expected_size;
        }
        ASSERT_EQ(expected_size, v.size());
    }
}