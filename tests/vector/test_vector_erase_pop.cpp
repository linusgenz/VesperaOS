// test_vector_erase_pop.cpp
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

// test_vector_erase_pop.cpp
// VesperaOS — Vector<T> Tests: erase / erase_value / pop / pop_back

#include "../framework/test_framework.h"
#include <vector.h>

// erase

TEST(Vector_Erase, EraseFirst, "erase(0) removes the first element, remainder shifts correctly") {
    Vector<int> v;
    v.push_back(10); v.push_back(20); v.push_back(30);
    v.erase(0);
    ASSERT_EQ(static_cast<size_t>(2), v.size());
    ASSERT_EQ(20, v[0]);
    ASSERT_EQ(30, v[1]);
}

TEST(Vector_Erase, EraseLast, "erase(size-1) removes the last element") {
    Vector<int> v;
    v.push_back(1); v.push_back(2); v.push_back(3);
    v.erase(2);
    ASSERT_EQ(static_cast<size_t>(2), v.size());
    ASSERT_EQ(1, v[0]);
    ASSERT_EQ(2, v[1]);
}

TEST(Vector_Erase, EraseMiddle, "erase(1) shifts tail left") {
    Vector<int> v;
    v.push_back(100); v.push_back(200); v.push_back(300); v.push_back(400);
    v.erase(1);
    ASSERT_EQ(static_cast<size_t>(3), v.size());
    ASSERT_EQ(100, v[0]);
    ASSERT_EQ(300, v[1]);
    ASSERT_EQ(400, v[2]);
}

TEST(Vector_Erase, EraseOnlyElement, "erase(0) on single-element Vector leaves it empty") {
    Vector<int> v;
    v.push_back(42);
    v.erase(0);
    ASSERT_EQ(static_cast<size_t>(0), v.size());
    ASSERT_TRUE(v.empty());
}

TEST(Vector_Erase, EraseAllSequentially, "Erasing index 0 repeatedly empties the vector in order") {
    Vector<int> v;
    for (int i = 1; i <= 5; ++i) v.push_back(i);
    for (int i = 1; i <= 5; ++i) {
        ASSERT_EQ(i, v[0]);
        v.erase(0);
    }
    ASSERT_TRUE(v.empty());
}

TEST(Vector_Erase, NonTrivialDestructorCalledOnErase, "Erasing a non-trivial element calls its destructor") {
    struct Tracker {
        int* destroyed;
        Tracker(int* d) : destroyed(d) {}
        Tracker(const Tracker& o) : destroyed(o.destroyed) {}
        ~Tracker() { if (destroyed) ++(*destroyed); }
        bool operator==(const Tracker& o) const { return destroyed == o.destroyed; }
    };

    int count = 0;
    {
        Vector<Tracker> v;
        Tracker t(&count);
        v.push_back(t); v.push_back(t); v.push_back(t);
        count = 0; // reset: ignore copy/move overhead from push_back
        v.erase(1);
        ASSERT_GE(count, 1);
    }
}

// erase_value

TEST(Vector_EraseValue, FindsAndRemovesFirst, "erase_value removes the first matching element and returns true") {
    Vector<int> v;
    v.push_back(5); v.push_back(10); v.push_back(5); v.push_back(20);
    bool result = v.erase_value(5);
    ASSERT_TRUE(result);
    ASSERT_EQ(static_cast<size_t>(3), v.size());
    ASSERT_EQ(10, v[0]); // first 5 gone
    ASSERT_EQ(5,  v[1]); // second 5 still there
    ASSERT_EQ(20, v[2]);
}

TEST(Vector_EraseValue, ReturnsFalseWhenNotFound, "erase_value returns false for a value not in the vector") {
    Vector<int> v;
    v.push_back(1); v.push_back(2); v.push_back(3);
    ASSERT_FALSE(v.erase_value(99));
    ASSERT_EQ(static_cast<size_t>(3), v.size());
}

TEST(Vector_EraseValue, EmptyVectorReturnsFalse, "erase_value on empty vector returns false") {
    Vector<int> v;
    ASSERT_FALSE(v.erase_value(0));
}

TEST(Vector_EraseValue, RemovesOnlyElement, "erase_value on a single-element match empties the vector") {
    Vector<int> v;
    v.push_back(7);
    ASSERT_TRUE(v.erase_value(7));
    ASSERT_TRUE(v.empty());
}

TEST(Vector_EraseValue, RemovesLastElement, "erase_value correctly removes the last element") {
    Vector<int> v;
    v.push_back(1); v.push_back(2); v.push_back(3);
    ASSERT_TRUE(v.erase_value(3));
    ASSERT_EQ(static_cast<size_t>(2), v.size());
    ASSERT_EQ(1, v[0]);
    ASSERT_EQ(2, v[1]);
}

TEST(Vector_EraseValue, DoesNotRemoveDuplicates, "erase_value removes only the first occurrence") {
    Vector<int> v;
    v.push_back(4); v.push_back(4); v.push_back(4);
    v.erase_value(4);
    ASSERT_EQ(static_cast<size_t>(2), v.size());
    ASSERT_EQ(4, v[0]);
    ASSERT_EQ(4, v[1]);
}

// pop

TEST(Vector_Pop, PopDecreasesSize, "pop() reduces size by 1") {
    Vector<int> v;
    v.push_back(1); v.push_back(2); v.push_back(3);
    v.pop();
    ASSERT_EQ(static_cast<size_t>(2), v.size());
}

TEST(Vector_Pop, PopDoesNotAlterRemaining, "pop() does not modify remaining elements") {
    Vector<int> v;
    v.push_back(10); v.push_back(20); v.push_back(30);
    v.pop();
    ASSERT_EQ(10, v[0]);
    ASSERT_EQ(20, v[1]);
}

TEST(Vector_Pop, PopAllElements, "Repeated pop() empties the vector") {
    Vector<int> v;
    for (int i = 0; i < 5; ++i) v.push_back(i);
    for (int i = 0; i < 5; ++i) v.pop();
    ASSERT_TRUE(v.empty());
}

// pop_back

TEST(Vector_PopBack, ReturnsRemovedValue, "pop_back() returns the last element") {
    Vector<int> v;
    v.push_back(11); v.push_back(22); v.push_back(33);
    int val = v.pop_back();
    ASSERT_EQ(33, val);
    ASSERT_EQ(static_cast<size_t>(2), v.size());
}

TEST(Vector_PopBack, SequentialPops, "Repeated pop_back() returns values in LIFO order") {
    Vector<int> v;
    for (int i = 1; i <= 5; ++i) v.push_back(i * 10);
    for (int i = 5; i >= 1; --i) {
        ASSERT_EQ(i * 10, v.pop_back());
    }
    ASSERT_TRUE(v.empty());
}

TEST(Vector_PopBack, SingleElementPop, "pop_back() on one-element vector returns that element and empties the vector") {
    Vector<int> v;
    v.push_back(99);
    int val = v.pop_back();
    ASSERT_EQ(99, val);
    ASSERT_TRUE(v.empty());
}

TEST(Vector_PopBack, NonTrivialType, "pop_back() on non-trivial type returns correct value") {
    struct Box {
        int x;
        Box(int v) : x(v) {}
        Box(const Box&) = default;
        ~Box() {}
        bool operator==(const Box& o) const { return x == o.x; }
    };
    Vector<Box> v;
    v.push_back(Box(42));
    v.push_back(Box(84));
    Box b = v.pop_back();
    ASSERT_EQ(84, b.x);
    ASSERT_EQ(static_cast<size_t>(1), v.size());
}