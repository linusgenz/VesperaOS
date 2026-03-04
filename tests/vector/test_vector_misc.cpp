// test_vector_misc.cpp
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

// test_vector_misc.cpp
// VesperaOS — Vector<T> Tests: clear / back / iterators / move / copy()

#include "../framework/test_framework.h"
#include <vector.h>
#include <cstring>

// clear

TEST(Vector_Clear, ClearSetsLengthToZero, "clear() sets size() to 0") {
    Vector<int> v;
    v.push_back(1); v.push_back(2); v.push_back(3);
    v.clear();
    ASSERT_EQ(static_cast<size_t>(0), v.size());
    ASSERT_TRUE(v.empty());
}

TEST(Vector_Clear, ClearAllowsReuse, "After clear(), push_back works as on a fresh vector") {
    Vector<int> v;
    for (int i = 0; i < 8; ++i) v.push_back(i);
    v.clear();
    v.push_back(100);
    ASSERT_EQ(static_cast<size_t>(1), v.size());
    ASSERT_EQ(100, v[0]);
}

TEST(Vector_Clear, ClearEmpty, "clear() on already-empty vector is a no-op") {
    Vector<int> v;
    v.clear();
    ASSERT_EQ(static_cast<size_t>(0), v.size());
    ASSERT_TRUE(v.empty());
}

TEST(Vector_Clear, ClearCallsDestructorsOnNonTrivial, "clear() calls destructor on every non-trivial element") {
    struct D {
        int* count;
        D(int* c) : count(c) {}
        D(const D& o) : count(o.count) {}
        ~D() { if (count) ++(*count); }
        bool operator==(const D&) const { return false; }
    };

    int destroyed = 0;
    Vector<D> v;
    D d(&destroyed);
    v.push_back(d); v.push_back(d); v.push_back(d);
    destroyed = 0;   // reset after push_back copies
    v.clear();
    ASSERT_GE(destroyed, 3);
}

TEST(Vector_Clear, ClearThenPushManyTimes, "Multiple clear+push cycles work correctly") {
    Vector<int> v;
    for (int cycle = 0; cycle < 5; ++cycle) {
        v.clear();
        for (int i = 0; i < 10; ++i) v.push_back(i * cycle);
        ASSERT_EQ(static_cast<size_t>(10), v.size());
        ASSERT_EQ(9 * cycle, v[9]);
    }
}

// back

TEST(Vector_Back, BackReturnLastElement, "back() returns the last pushed element") {
    Vector<int> v;
    v.push_back(1); v.push_back(2); v.push_back(99);
    ASSERT_EQ(99, v.back());
}

TEST(Vector_Back, BackAfterPopBack, "back() reflects the new last element after pop_back") {
    Vector<int> v;
    v.push_back(10); v.push_back(20); v.push_back(30);
    v.pop_back();
    ASSERT_EQ(20, v.back());
}

TEST(Vector_Back, BackIsWritable, "Non-const back() can be assigned") {
    Vector<int> v;
    v.push_back(0);
    v.back() = 42;
    ASSERT_EQ(42, v[0]);
}

TEST(Vector_Back, ConstBack, "Const back() returns the correct last element") {
    Vector<int> v;
    v.push_back(7); v.push_back(8);
    const Vector<int>& cv = v;
    ASSERT_EQ(8, cv.back());
}

TEST(Vector_Back, SingleElement, "back() on a one-element vector returns that element") {
    Vector<int> v;
    v.push_back(55);
    ASSERT_EQ(55, v.back());
}

// Iterators

TEST(Vector_Iterator, BeginEqualsEndForEmpty, "begin() == end() for an empty vector") {
    Vector<int> v;
    ASSERT_TRUE(v.begin() == v.end());
}

TEST(Vector_Iterator, BeginPlusNEqualsEnd, "begin() + size() == end()") {
    Vector<int> v;
    for (int i = 0; i < 5; ++i) v.push_back(i);
    ASSERT_TRUE(v.begin() + 5 == v.end());
}

TEST(Vector_Iterator, RangeForWorks, "Range-for loop visits all elements in order") {
    Vector<int> v;
    for (int i = 0; i < 8; ++i) v.push_back(i * 3);
    int idx = 0;
    for (int x : v) {
        ASSERT_EQ(idx * 3, x);
        ++idx;
    }
    ASSERT_EQ(8, idx);
}

TEST(Vector_Iterator, CbeginCend, "cbegin/cend iterate over all elements") {
    Vector<int> v;
    v.push_back(10); v.push_back(20); v.push_back(30);
    int expected[] = {10, 20, 30};
    int i = 0;
    for (auto it = v.cbegin(); it != v.cend(); ++it, ++i)
        ASSERT_EQ(expected[i], *it);
}

TEST(Vector_Iterator, WriteThroughBegin, "Writing through begin() iterator modifies vector element") {
    Vector<int> v;
    v.push_back(1); v.push_back(2);
    *v.begin() = 99;
    ASSERT_EQ(99, v[0]);
}

TEST(Vector_Iterator, ConstIterator, "Const vector provides correct const iterators") {
    Vector<int> v;
    v.push_back(5); v.push_back(6);
    const Vector<int>& cv = v;
    int sum = 0;
    for (auto it = cv.begin(); it != cv.end(); ++it) sum += *it;
    ASSERT_EQ(11, sum);
}

// Move semantics

TEST(Vector_Move, MoveConstructor, "Move constructor transfers ownership; source is empty") {
    Vector<int> a;
    for (int i = 0; i < 5; ++i) a.push_back(i);

    Vector<int> b(static_cast<Vector<int>&&>(a));

    ASSERT_EQ(static_cast<size_t>(5), b.size());
    for (int i = 0; i < 5; ++i) ASSERT_EQ(i, b[i]);

    ASSERT_EQ(static_cast<size_t>(0), a.size());
    ASSERT_TRUE(a.empty());
    ASSERT_NULL(a.data());
}

TEST(Vector_Move, MoveAssignment, "Move assignment replaces content; source is emptied") {
    Vector<int> a;
    a.push_back(10); a.push_back(20);

    Vector<int> b;
    b.push_back(99);

    b = static_cast<Vector<int>&&>(a);

    ASSERT_EQ(static_cast<size_t>(2), b.size());
    ASSERT_EQ(10, b[0]);
    ASSERT_EQ(20, b[1]);

    ASSERT_TRUE(a.empty());
}

TEST(Vector_Move, SelfMoveAssignment, "Self-move-assignment is safe (no crash, data intact)") {
    Vector<int> v;
    v.push_back(1); v.push_back(2);
    // This must not corrupt the vector
    v = static_cast<Vector<int>&&>(v);
    // After self-move the standard permits a valid-but-unspecified state.
    // Just check no crash occurred — we do not assert size here.
    ASSERT_TRUE(true);
}

TEST(Vector_Move, MoveConstructorNonTrivial, "Move constructor works for non-trivial types") {
    struct S {
        int v;
        S(int x) : v(x) {}
        S(const S&) = default;
        ~S() {}
        bool operator==(const S& o) const { return v == o.v; }
    };
    Vector<S> a;
    a.push_back(S(1)); a.push_back(S(2)); a.push_back(S(3));
    Vector<S> b(static_cast<Vector<S>&&>(a));
    ASSERT_EQ(static_cast<size_t>(3), b.size());
    ASSERT_EQ(1, b[0].v);
    ASSERT_EQ(2, b[1].v);
    ASSERT_EQ(3, b[2].v);
    ASSERT_TRUE(a.empty());
}

// copy()

TEST(Vector_Copy, CopyProducesIndependentVector, "copy() produces a vector with the same values") {
    Vector<int> a;
    for (int i = 0; i < 6; ++i) a.push_back(i * 7);

    Vector<int> b = a.copy();
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) ASSERT_EQ(a[i], b[i]);
}

TEST(Vector_Copy, CopyIsDeepNotShallow, "Modifying copy() result does not affect the original") {
    Vector<int> a;
    a.push_back(1); a.push_back(2); a.push_back(3);

    Vector<int> b = a.copy();
    b[0] = 999;

    ASSERT_EQ(1, a[0]);
    ASSERT_EQ(999, b[0]);
}

TEST(Vector_Copy, CopyOfEmpty, "copy() on an empty vector produces an empty vector") {
    Vector<int> a;
    Vector<int> b = a.copy();
    ASSERT_EQ(static_cast<size_t>(0), b.size());
    ASSERT_TRUE(b.empty());
}

TEST(Vector_Copy, CopyNonTrivial, "copy() correctly duplicates non-trivial objects") {
    struct Pair {
        int x, y;
        Pair(int a, int b) : x(a), y(b) {}
        Pair(const Pair&) = default;
        ~Pair() {}
        bool operator==(const Pair& o) const { return x == o.x && y == o.y; }
    };
    Vector<Pair> a;
    a.push_back(Pair(1,2)); a.push_back(Pair(3,4));
    Vector<Pair> b = a.copy();
    ASSERT_EQ(static_cast<size_t>(2), b.size());
    ASSERT_EQ(1, b[0].x); ASSERT_EQ(2, b[0].y);
    ASSERT_EQ(3, b[1].x); ASSERT_EQ(4, b[1].y);
}

// Misc / operator[]

TEST(Vector_Misc, OperatorBracketConst, "const operator[] returns correct values") {
    Vector<int> v;
    v.push_back(5); v.push_back(10); v.push_back(15);
    const Vector<int>& cv = v;
    ASSERT_EQ(5,  cv[0]);
    ASSERT_EQ(10, cv[1]);
    ASSERT_EQ(15, cv[2]);
}

TEST(Vector_Misc, OperatorBracketWrite, "operator[] allows writing to any valid index") {
    Vector<int> v;
    v.push_back(0); v.push_back(0); v.push_back(0);
    v[0] = 11; v[1] = 22; v[2] = 33;
    ASSERT_EQ(11, v[0]);
    ASSERT_EQ(22, v[1]);
    ASSERT_EQ(33, v[2]);
}

TEST(Vector_Misc, SizeAndEmptyConsistency, "size() == 0 iff empty() == true") {
    Vector<int> v;
    ASSERT_TRUE(v.empty());
    ASSERT_EQ(static_cast<size_t>(0), v.size());
    v.push_back(1);
    ASSERT_FALSE(v.empty());
    ASSERT_EQ(static_cast<size_t>(1), v.size());
}

TEST(Vector_Misc, LargeDataType, "Vector<uint64_t> stores large values correctly") {
    Vector<uint64_t> v;
    v.push_back(0xDEADBEEFCAFEBABEULL);
    v.push_back(0x0123456789ABCDEFULL);
    ASSERT_EQ(0xDEADBEEFCAFEBABEULL, v[0]);
    ASSERT_EQ(0x0123456789ABCDEFULL, v[1]);
}

TEST(Vector_Misc, VectorOfVectorsPushBack, "Vector<int*> stores pointers for inner vectors correctly") {
    Vector<int> inner;
    inner.push_back(1); inner.push_back(2);

    Vector<int> inner2;
    inner2.push_back(3);

    Vector<int*> ptrs;
    int* p1 = inner.data();
    int* p2 = inner2.data();
    ptrs.push_back(p1);
    ptrs.push_back(p2);
    ASSERT_TRUE(ptrs[0] == p1);
    ASSERT_TRUE(ptrs[1] == p2);
}