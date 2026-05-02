// test_vector_panic.cpp
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

#include <klib/vector.h>
#include "../framework/test_framework.h"

TEST(Vector_Panic, EraseOutOfRangePanics, "erase() beyond size triggers system_panic") {
    Vector<int> v;
    v.push_back(1);

    ASSERT_PANICS({ v.erase(5); });
    ASSERT_EQ(-KERANGE, g_panic_code);
}

TEST(Vector_Panic, BackOnEmptyPanics, "back() on empty vector triggers system_panic") {
    Vector<int> v;
    ASSERT_PANICS({ v.back(); });
}

TEST(Vector_Panic, PopOnEmptyPanics, "pop() on empty vector triggers system_panic") {
    Vector<int> v;
    ASSERT_PANICS({ v.pop(); });
}

TEST(Vector_Panic, PopBackOnEmptyPanics, "pop_back() on empty vector triggers system_panic") {
    Vector<int> v;
    ASSERT_PANICS({ v.pop_back(); });
}