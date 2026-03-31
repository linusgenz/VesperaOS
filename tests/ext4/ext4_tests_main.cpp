// ext4_tests_main.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 25.03.26.
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

#include <cassert>
#include <cstdio>
#include "ext4_fixture.h"

#include "test_ext4_vfs.cpp"
#include "test_ext4_fileio.cpp"
#include "test_ext4_create_delete.cpp"
#include "test_ext4_rename.cpp"
#include "test_ext4_stat.cpp"
#include "test_ext4_truncate.cpp"
#include "test_ext4_fs.cpp"

TEST_MAIN()
