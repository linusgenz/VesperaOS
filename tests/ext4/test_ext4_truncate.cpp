// test_ext4_truncate.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 30.03.26.
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

#include "ext4_fixture.h"
#include <cstring>
#include <vector>

TEST(Ext4_Truncate, ShrinkReducesReadableBytes,
     "After truncate(n), read() returns at most n bytes") {
    WITH_EXT4(f);

    // Use trunctest.txt which has 29 bytes of known text.
    EXT4_FIND(f, "trunctest.txt", node);
    NodeGuard guard(node);

    constexpr usize new_size = 10;
    ASSERT_EQ(0, f.truncate(node, new_size));

    char buf[64] = {};
    isize r = f.read(node, 0, sizeof(buf), buf);
    ASSERT_EQ(static_cast<isize>(new_size), r);
}

TEST(Ext4_Truncate, ShrinkUpdatesNodeSize,
     "After truncate(n), VfsNode::size == n") {
    WITH_EXT4(f);
    EXT4_FIND(f, "trunctest.txt", node);
    NodeGuard guard(node);

    constexpr usize new_size = 12;
    ASSERT_EQ(0, f.truncate(node, new_size));
    ASSERT_EQ(new_size, node->size);
}

TEST(Ext4_Truncate, ShrinkPreservesLeadingBytes,
     "After shrink, the retained bytes match the original file content") {
    WITH_EXT4(f);
    EXT4_FIND(f, "trunctest.txt", node);
    NodeGuard guard(node);

    // "truncation test content here\n" → first 11 bytes = "truncation "
    constexpr usize new_size = 11;
    ASSERT_EQ(0, f.truncate(node, new_size));

    char buf[16] = {};
    isize r = f.read(node, 0, new_size, buf);
    ASSERT_EQ(static_cast<isize>(new_size), r);
    ASSERT_MEM_EQ("truncation ", buf, new_size);
}

TEST(Ext4_Truncate, ShrinkBinaryFile,
     "Shrinking binary.bin to 4096 bytes leaves the first block intact") {
    WITH_EXT4(f);
    EXT4_FIND(f, "binary.bin", node);
    NodeGuard guard(node);

    ASSERT_EQ(0, f.truncate(node, 4096));
    ASSERT_EQ(static_cast<usize>(4096), node->size);

    std::vector<uint8_t> buf(4096, 0);
    isize r = f.read(node, 0, 4096, buf.data());
    ASSERT_EQ(static_cast<isize>(4096), r);
    for (usize i = 0; i < 4096; ++i)
        ASSERT_EQ(Ext4Fixture::binary_pattern(i), buf[i]);
}

TEST(Ext4_Truncate, ShrinkToOneByte,
     "Truncating to 1 byte leaves exactly one readable byte") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);

    ASSERT_EQ(0, f.truncate(node, 1));
    ASSERT_EQ(static_cast<usize>(1), node->size);

    char c = 0;
    isize r = f.read(node, 0, 1, &c);
    ASSERT_EQ(static_cast<isize>(1), r);
    ASSERT_EQ('h', c);  // first byte of "hello from ext4\n"
}

TEST(Ext4_Truncate, TruncateToZeroMakesFileSizeZero,
     "truncate(0) sets the file size to 0") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);

    ASSERT_EQ(0, f.truncate(node, 0));
    ASSERT_EQ(static_cast<usize>(0), node->size);
}

TEST(Ext4_Truncate, TruncateToZeroMakesReadReturnZero,
     "After truncate(0), read() returns 0 bytes") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);

    ASSERT_EQ(0, f.truncate(node, 0));

    char buf[32] = {};
    isize r = f.read(node, 0, sizeof(buf), buf);
    ASSERT_EQ(static_cast<isize>(0), r);
}

TEST(Ext4_Truncate, TruncateToZeroThenWrite,
     "After truncate(0), the file can be written with new data") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);

    ASSERT_EQ(0, f.truncate(node, 0));

    constexpr char data[] = "new content after truncate";
    constexpr usize len   = sizeof(data) - 1;
    isize w = f.write(node, 0, len, data);
    ASSERT_EQ(static_cast<isize>(len), w);

    char buf[64] = {};
    isize r = f.read(node, 0, len, buf);
    ASSERT_EQ(static_cast<isize>(len), r);
    ASSERT_MEM_EQ(data, buf, len);
}

TEST(Ext4_Truncate, TruncateToZeroOnLargeFile,
     "truncate(0) on big.bin (65536 bytes) shrinks it to zero") {
    WITH_EXT4(f);
    EXT4_FIND(f, "big.bin", node);
    NodeGuard guard(node);

    ASSERT_EQ(0, f.truncate(node, 0));
    ASSERT_EQ(static_cast<usize>(0), node->size);

    char buf[8] = {};
    isize r = f.read(node, 0, sizeof(buf), buf);
    ASSERT_EQ(static_cast<isize>(0), r);
}

TEST(Ext4_Truncate, TruncateToSameSizeIsNoOp,
     "truncate(current_size) leaves content unchanged") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);

    ASSERT_EQ(0, f.truncate(node, HELLO_TXT_SIZE));
    ASSERT_EQ(HELLO_TXT_SIZE, node->size);

    char buf[32] = {};
    isize r = f.read(node, 0, HELLO_TXT_SIZE, buf);
    ASSERT_EQ(static_cast<isize>(HELLO_TXT_SIZE), r);
    ASSERT_MEM_EQ("hello from ext4\n", buf, HELLO_TXT_SIZE);
}

TEST(Ext4_Truncate, ExtendIncreasesSize,
     "truncate(larger) extends the file and updates VfsNode::size") {
    WITH_EXT4(f);

    ASSERT_EQ(0, f.create(f.root, "extend.txt"));
    VfsNode* node = f.find_root("extend.txt");
    ASSERT_NOT_NULL(node);
    NodeGuard guard(node);

    // Write 4 bytes, then extend to 16.
    ASSERT_GE(f.write(node, 0, 4, "AAAA"), static_cast<isize>(1));
    ASSERT_EQ(0, f.truncate(node, 16));
    ASSERT_EQ(static_cast<usize>(16), node->size);
}

TEST(Ext4_Truncate, ExtendPreservesOriginalContent,
     "After extending, the original bytes at offset 0 are still readable") {
    WITH_EXT4(f);

    ASSERT_EQ(0, f.create(f.root, "ext_preserve.txt"));
    VfsNode* node = f.find_root("ext_preserve.txt");
    ASSERT_NOT_NULL(node);
    NodeGuard guard(node);

    ASSERT_GE(f.write(node, 0, 4, "AAAA"), static_cast<isize>(1));
    ASSERT_EQ(0, f.truncate(node, 4096));

    char buf[4] = {};
    isize r = f.read(node, 0, 4, buf);
    ASSERT_EQ(static_cast<isize>(4), r);
    ASSERT_MEM_EQ("AAAA", buf, 4);
}

TEST(Ext4_Truncate, TruncateNullNodeFails,
     "truncate() on a null VfsNode returns a non-zero error code") {
    WITH_EXT4(f);
    ASSERT_NE(0, f.truncate(nullptr, 0));
}

TEST(Ext4_Truncate, TruncateDirectoryFails,
     "truncate() on a directory node returns a non-zero error code") {
    WITH_EXT4(f);
    EXT4_FIND(f, "subdir", dir);
    NodeGuard guard(dir);

    ASSERT_NE(0, f.truncate(dir, 0));
}

TEST(Ext4_Truncate, FsTruncateShrinksFile,
     "fs->truncate() via raw API reduces readable byte count") {
    WITH_EXT4(f);

    VfsNode* node = f.find_root("trunctest.txt");
    ASSERT_NOT_NULL(node);
    auto* nd = static_cast<Ext4Node*>(node->internal_data);
    u32 inode_no = nd->inode;
    Ext4Fixture::free_node(node);

    ASSERT_TRUE(f.fs->truncate(inode_no, 5));

    char buf[32] = {};
    auto r_res = f.fs->read_file(inode_no, 0, sizeof(buf), buf, false);
    ASSERT_TRUE(r_res.is_ok());
    // First 5 bytes of "truncation test content here\n" = "trunc"
    ASSERT_MEM_EQ("trunc", buf, 5);
}