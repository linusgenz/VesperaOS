// test_vector_push_back.cpp
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

#include "../framework/test_framework.h"
#include <klib/vector.h>
#include <cstring>

// Construction

TEST(Vector_PushBack, DefaultConstruct, "Default-constructed Vector has size 0 and is empty") {
    Vector<int> v;
    ASSERT_EQ(static_cast<size_t>(0), v.size());
    ASSERT_TRUE(v.empty());
}

TEST(Vector_PushBack, CustomCapacity, "Vector constructed with capacity=1 starts empty") {
    Vector<int> v(1);
    ASSERT_EQ(static_cast<size_t>(0), v.size());
    ASSERT_TRUE(v.empty());
    ASSERT_NOT_NULL(v.data());
}

TEST(Vector_PushBack, LargeInitialCapacity, "Vector(1000) allocates up-front and starts empty") {
    Vector<int> v(1000);
    ASSERT_EQ(static_cast<size_t>(0), v.size());
    ASSERT_NOT_NULL(v.data());
}

// push_back — basic

TEST(Vector_PushBack, SinglePush, "push_back increases size to 1") {
    Vector<int> v;
    v.push_back(42);
    ASSERT_EQ(static_cast<size_t>(1), v.size());
    ASSERT_FALSE(v.empty());
}

TEST(Vector_PushBack, ValuePreserved, "Value pushed is accessible at index 0") {
    Vector<int> v;
    v.push_back(99);
    ASSERT_EQ(99, v[0]);
}

TEST(Vector_PushBack, MultiplePushes, "Multiple push_back calls preserve order") {
    Vector<int> v;
    for (int i = 0; i < 10; ++i) v.push_back(i);
    ASSERT_EQ(static_cast<size_t>(10), v.size());
    for (int i = 0; i < 10; ++i) ASSERT_EQ(i, v[i]);
}

TEST(Vector_PushBack, ZeroValue, "push_back(0) stores zero correctly") {
    Vector<int> v;
    v.push_back(0);
    ASSERT_EQ(0, v[0]);
}

TEST(Vector_PushBack, NegativeValue, "push_back stores negative integers correctly") {
    Vector<int> v;
    v.push_back(-1);
    v.push_back(-1000);
    ASSERT_EQ(-1, v[0]);
    ASSERT_EQ(-1000, v[1]);
}

TEST(Vector_PushBack, MaxIntValue, "push_back stores INT_MAX correctly") {
    Vector<int> v;
    v.push_back(2147483647);
    ASSERT_EQ(2147483647, v[0]);
}

// Capacity growth / resize

TEST(Vector_PushBack, GrowthBeyondInitialCapacity, "push_back beyond initial capacity triggers resize, data intact") {
    Vector<int> v(4);
    for (int i = 0; i < 16; ++i) v.push_back(i * 3);
    ASSERT_EQ(static_cast<size_t>(16), v.size());
    for (int i = 0; i < 16; ++i) ASSERT_EQ(i * 3, v[i]);
}

TEST(Vector_PushBack, GrowthKeepsAllValues, "All values survive multiple resize cycles") {
    Vector<int> v(1);
    for (int i = 0; i < 128; ++i) v.push_back(i);
    ASSERT_EQ(static_cast<size_t>(128), v.size());
    for (int i = 0; i < 128; ++i) ASSERT_EQ(i, v[i]);
}

TEST(Vector_PushBack, DataPointerValid, "data() is non-null after push_back") {
    Vector<int> v;
    v.push_back(1);
    ASSERT_NOT_NULL(v.data());
}

TEST(Vector_PushBack, DataPointerMatchesIndex, "data()[i] matches v[i] after push_back") {
    Vector<int> v;
    for (int i = 0; i < 8; ++i) v.push_back(i * 10);
    for (size_t i = 0; i < v.size(); ++i) ASSERT_EQ(v[i], v.data()[i]);
}

// push_back with non-trivial types

struct Counted {
    static int constructions;
    static int destructions;
    int val;
    explicit Counted(int v) : val(v) { ++constructions; }
    Counted(const Counted& o) : val(o.val) { ++constructions; }
    ~Counted() { ++destructions; }
    bool operator==(const Counted& o) const { return val == o.val; }
};
int Counted::constructions = 0;
int Counted::destructions  = 0;

TEST(Vector_PushBack, NonTrivialTypeStored, "push_back stores non-trivial objects correctly") {
    Vector<Counted> v;
    v.push_back(Counted(7));
    v.push_back(Counted(13));
    ASSERT_EQ(static_cast<size_t>(2), v.size());
    ASSERT_EQ(7,  v[0].val);
    ASSERT_EQ(13, v[1].val);
}

TEST(Vector_PushBack, NonTrivialDestructorCalledOnDestroy, "Destructor of non-trivial type is called for every element") {
    Counted::destructions = 0;
    {
        Vector<Counted> v;
        v.push_back(Counted(1));
        v.push_back(Counted(2));
        v.push_back(Counted(3));
        // destructor fires here
    }

    ASSERT_GE(Counted::destructions, 3);
}

TEST(Vector_PushBack, NonTrivialGrowthPreservesValues, "Non-trivial values survive a resize") {
    Vector<Counted> v(2);
    for (int i = 0; i < 20; ++i) {
        Counted c(i * 5);
        v.push_back(c);
    }
    ASSERT_EQ(static_cast<size_t>(20), v.size());
    for (int i = 0; i < 20; ++i) ASSERT_EQ(i * 5, v[i].val);
}

// push_back with pointer type

TEST(Vector_PushBack, PointerType, "Vector<int*> stores pointers correctly") {
    int a = 1, b = 2, c = 3;
    Vector<int*> v;
    v.push_back(&a);
    v.push_back(&b);
    v.push_back(&c);
    ASSERT_TRUE(v[0] == &a);
    ASSERT_TRUE(v[1] == &b);
    ASSERT_TRUE(v[2] == &c);
    ASSERT_EQ(1, *v[0]);
}

// StressTest

TEST(Vector_PushBack, Stress1000, "push_back 1000 elements in order, all readable") {
    Vector<int> v;
    for (int i = 0; i < 1000; ++i) v.push_back(i);
    ASSERT_EQ(static_cast<size_t>(1000), v.size());
    for (int i = 0; i < 1000; ++i) ASSERT_EQ(i, v[i]);
}
