// test_ext4_fileio.cpp
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
#include <vector>
#include <cstring>

TEST(Ext4_FileIO, ReadHelloTxt,
     "read() returns the exact content of hello.txt") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);

    char buf[32] = {};
    isize n = f.read(node, 0, sizeof(buf), buf);
    ASSERT_EQ(static_cast<isize>(HELLO_TXT_SIZE), n);
    ASSERT_MEM_EQ("hello from ext4\n", buf, HELLO_TXT_SIZE);
}

TEST(Ext4_FileIO, ReadHelloTxtWithOffset,
     "read() from offset 6 returns the tail of hello.txt") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);

    // "hello from ext4\n"
    //       ^-- offset 6 → "from ext4\n"
    char buf[16] = {};
    isize n = f.read(node, 6, 10, buf);
    ASSERT_EQ(static_cast<isize>(10), n);
    ASSERT_MEM_EQ("from ext4\n", buf, 10);
}

TEST(Ext4_FileIO, ReadHelloTxtLastByte,
     "read() at offset size-1 returns exactly 1 byte") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);

    char c = 0;
    isize n = f.read(node, HELLO_TXT_SIZE - 1, 1, &c);
    ASSERT_EQ(static_cast<isize>(1), n);
    ASSERT_EQ('\n', c);
}

TEST(Ext4_FileIO, ReadBeyondEOFReturnsZeroOrClamped,
     "read() with offset at file size returns 0 bytes") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);

    char buf[8] = {};
    isize n = f.read(node, HELLO_TXT_SIZE, sizeof(buf), buf);
    ASSERT_EQ(static_cast<isize>(0), n);
}

TEST(Ext4_FileIO, ReadFarBeyondEOF,
     "read() with offset far past EOF returns 0") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);

    char buf[8] = {};
    isize n = f.read(node, 99999, sizeof(buf), buf);
    ASSERT_EQ(static_cast<isize>(0), n);
}

TEST(Ext4_FileIO, ReadPartialLargerThanFile,
     "read() requesting more bytes than the file size returns only fileSize bytes") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);

    char buf[256] = {};
    isize n = f.read(node, 0, sizeof(buf), buf);
    ASSERT_EQ(static_cast<isize>(HELLO_TXT_SIZE), n);
}

TEST(Ext4_FileIO, ReadEmptyFileReturnsZero,
     "read() on an empty file returns 0 bytes") {
    WITH_EXT4(f);
    EXT4_FIND(f, "empty.txt", node);
    NodeGuard guard(node);

    char buf[8] = {};
    isize n = f.read(node, 0, sizeof(buf), buf);
    ASSERT_EQ(static_cast<isize>(0), n);
}

TEST(Ext4_FileIO, ReadDirectoryFails,
     "read() on a directory node returns an error (<0)") {
    WITH_EXT4(f);
    EXT4_FIND(f, "subdir", dir);
    NodeGuard guard(dir);

    char buf[32] = {};
    isize n = f.read(dir, 0, sizeof(buf), buf);
    ASSERT_TRUE(n < 0);
}

TEST(Ext4_FileIO, ReadNullNodeFails,
     "read() with null node returns -1") {
    WITH_EXT4(f);
    char buf[8] = {};
    isize n = f.read(nullptr, 0, sizeof(buf), buf);
    ASSERT_EQ(static_cast<isize>(-1), n);
}

TEST(Ext4_FileIO, ReadBinaryBinFirstBlock,
     "First 4096 bytes of binary.bin match pattern i%256") {
    WITH_EXT4(f);
    EXT4_FIND(f, "binary.bin", node);
    NodeGuard guard(node);

    std::vector<uint8_t> buf(4096, 0);
    isize n = f.read(node, 0, 4096, buf.data());
    ASSERT_EQ(static_cast<isize>(4096), n);

    for (usize i = 0; i < 4096; ++i)
        ASSERT_EQ(Ext4Fixture::binary_pattern(i), buf[i]);
}

TEST(Ext4_FileIO, ReadBinaryBinSecondBlock,
     "Second 4096 bytes of binary.bin match pattern i%256 (continued)") {
    WITH_EXT4(f);
    EXT4_FIND(f, "binary.bin", node);
    NodeGuard guard(node);

    std::vector<uint8_t> buf(4096, 0);
    isize n = f.read(node, 4096, 4096, buf.data());
    ASSERT_EQ(static_cast<isize>(4096), n);

    for (usize i = 0; i < 4096; ++i)
        ASSERT_EQ(Ext4Fixture::binary_pattern(4096 + i), buf[i]);
}

TEST(Ext4_FileIO, ReadBinaryBinFull,
     "Full 8192-byte read of binary.bin matches expected pattern") {
    WITH_EXT4(f);
    EXT4_FIND(f, "binary.bin", node);
    NodeGuard guard(node);

    std::vector<uint8_t> buf(BINARY_BIN_SIZE, 0);
    isize n = f.read(node, 0, BINARY_BIN_SIZE, buf.data());
    ASSERT_EQ(static_cast<isize>(BINARY_BIN_SIZE), n);

    for (usize i = 0; i < BINARY_BIN_SIZE; ++i)
        ASSERT_EQ(Ext4Fixture::binary_pattern(i), buf[i]);
}

TEST(Ext4_FileIO, ReadBinaryBinCrossBoundary,
     "Read spanning both blocks of binary.bin (offset 4090, len 12) is correct") {
    WITH_EXT4(f);
    EXT4_FIND(f, "binary.bin", node);
    NodeGuard guard(node);

    constexpr usize OFF = 4090;
    constexpr usize LEN = 12;  // spans block boundary at 4096
    uint8_t buf[LEN] = {};
    isize n = f.read(node, OFF, LEN, buf);
    ASSERT_EQ(static_cast<isize>(LEN), n);
    for (usize i = 0; i < LEN; ++i)
        ASSERT_EQ(Ext4Fixture::binary_pattern(OFF + i), buf[i]);
}

TEST(Ext4_FileIO, ReadBigBinFull,
     "Full 65536-byte read of big.bin matches expected pattern") {
    WITH_EXT4(f);
    EXT4_FIND(f, "big.bin", node);
    NodeGuard guard(node);

    std::vector<uint8_t> buf(BIG_BIN_SIZE, 0);
    isize n = f.read(node, 0, BIG_BIN_SIZE, buf.data());
    ASSERT_EQ(static_cast<isize>(BIG_BIN_SIZE), n);

    for (usize i = 0; i < BIG_BIN_SIZE; ++i)
        ASSERT_EQ(Ext4Fixture::big_pattern(i), buf[i]);
}

TEST(Ext4_FileIO, ReadBigBinAtOffset,
     "Offset read in the middle of big.bin returns correct bytes") {
    WITH_EXT4(f);
    EXT4_FIND(f, "big.bin", node);
    NodeGuard guard(node);

    constexpr usize OFF = 32768;
    constexpr usize LEN = 256;
    uint8_t buf[LEN] = {};
    isize n = f.read(node, OFF, LEN, buf);
    ASSERT_EQ(static_cast<isize>(LEN), n);
    for (usize i = 0; i < LEN; ++i)
        ASSERT_EQ(Ext4Fixture::big_pattern(OFF + i), buf[i]);
}

TEST(Ext4_FileIO, WriteAndReadBack,
     "Data written to a file can be read back verbatim") {
    WITH_EXT4(f);

    // Create a new file, write to it, read back.
    ASSERT_EQ(0, f.create(f.root, "write_test.txt"));

    VfsNode* node = f.find_root("write_test.txt");
    ASSERT_NOT_NULL(node);
    NodeGuard guard(node);

    constexpr char data[] = "VesperaOS ext4 write test";
    constexpr usize len   = sizeof(data) - 1;

    isize w = f.write(node, 0, len, data);
    ASSERT_EQ(static_cast<isize>(len), w);

    char buf[64] = {};
    isize r = f.read(node, 0, sizeof(buf), buf);
    ASSERT_EQ(static_cast<isize>(len), r);
    ASSERT_MEM_EQ(data, buf, len);
}

TEST(Ext4_FileIO, WriteAtOffset,
     "Writing at a non-zero offset leaves the preceding bytes untouched") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "offset_write.txt"));

    VfsNode* node = f.find_root("offset_write.txt");
    ASSERT_NOT_NULL(node);
    NodeGuard guard(node);

    // Write "AAAAAAAAAA" (10 bytes) then overwrite positions 3-5 with "BBB"
    ASSERT_GE(f.write(node, 0, 10, "AAAAAAAAAA"), static_cast<isize>(1));
    ASSERT_GE(f.write(node, 3,  3, "BBB"),        static_cast<isize>(1));

    char buf[16] = {};
    isize r = f.read(node, 0, 10, buf);
    ASSERT_EQ(static_cast<isize>(10), r);
    ASSERT_MEM_EQ("AAABBBAAAA", buf, 10);
}

TEST(Ext4_FileIO, WriteMultiBlock,
     "Writing 8192 bytes spanning two blocks round-trips correctly") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "multiblock.bin"));

    VfsNode* node = f.find_root("multiblock.bin");
    ASSERT_NOT_NULL(node);
    NodeGuard guard(node);

    std::vector<uint8_t> src(8192);
    for (usize i = 0; i < 8192; ++i) src[i] = static_cast<uint8_t>(i & 0xFF);

    isize w = f.write(node, 0, 8192, src.data());
    ASSERT_EQ(static_cast<isize>(8192), w);

    std::vector<uint8_t> dst(8192, 0);
    isize r = f.read(node, 0, 8192, dst.data());
    ASSERT_EQ(static_cast<isize>(8192), r);
    ASSERT_MEM_EQ(src.data(), dst.data(), 8192);
}

TEST(Ext4_FileIO, WriteLargeFile,
     "Writing a 65536-byte file round-trips correctly") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "large.bin"));

    VfsNode* node = f.find_root("large.bin");
    ASSERT_NOT_NULL(node);
    NodeGuard guard(node);

    std::vector<uint8_t> src(65536);
    for (usize i = 0; i < 65536; ++i)
        src[i] = static_cast<uint8_t>((i * 7 + 13) & 0xFF);

    isize w = f.write(node, 0, 65536, src.data());
    ASSERT_EQ(static_cast<isize>(65536), w);

    std::vector<uint8_t> dst(65536, 0);
    isize r = f.read(node, 0, 65536, dst.data());
    ASSERT_EQ(static_cast<isize>(65536), r);
    ASSERT_MEM_EQ(src.data(), dst.data(), 65536);
}

TEST(Ext4_FileIO, WriteAllByteValues,
     "Writing all 256 byte values and reading them back is lossless") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "bytes.bin"));

    VfsNode* node = f.find_root("bytes.bin");
    ASSERT_NOT_NULL(node);
    NodeGuard guard(node);

    uint8_t src[256];
    for (int i = 0; i < 256; ++i) src[i] = static_cast<uint8_t>(i);

    isize w = f.write(node, 0, 256, src);
    ASSERT_EQ(static_cast<isize>(256), w);

    uint8_t dst[256] = {};
    isize r = f.read(node, 0, 256, dst);
    ASSERT_EQ(static_cast<isize>(256), r);
    ASSERT_MEM_EQ(src, dst, 256);
}

TEST(Ext4_FileIO, WriteOverwriteSameSize,
     "Overwriting the entire content of a file with new data works") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "overwrite.txt"));

    VfsNode* node = f.find_root("overwrite.txt");
    ASSERT_NOT_NULL(node);
    NodeGuard guard(node);

    ASSERT_GE(f.write(node, 0, 6, "AAAAAA"), static_cast<isize>(1));
    ASSERT_GE(f.write(node, 0, 6, "BBBBBB"), static_cast<isize>(1));

    char buf[8] = {};
    isize r = f.read(node, 0, 6, buf);
    ASSERT_EQ(static_cast<isize>(6), r);
    ASSERT_MEM_EQ("BBBBBB", buf, 6);
}

TEST(Ext4_FileIO, WriteToExistingFile,
     "Writing to an existing image file (hello.txt) changes its content") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);

    constexpr char new_data[] = "modified content";
    constexpr usize new_len   = sizeof(new_data) - 1;

    isize w = f.write(node, 0, new_len, new_data);
    ASSERT_GE(w, static_cast<isize>(1));

    char buf[64] = {};
    isize r = f.read(node, 0, new_len, buf);
    ASSERT_EQ(static_cast<isize>(new_len), r);
    ASSERT_MEM_EQ(new_data, buf, new_len);
}

TEST(Ext4_FileIO, WriteToDirectoryFails,
     "write() on a directory node returns an error") {
    WITH_EXT4(f);
    EXT4_FIND(f, "subdir", dir);
    NodeGuard guard(dir);

    isize w = f.write(dir, 0, 4, "data");
    ASSERT_TRUE(w < 0);
}

TEST(Ext4_FileIO, WriteNullNodeFails,
     "write() with null node returns -1") {
    WITH_EXT4(f);
    isize w = f.write(nullptr, 0, 4, "data");
    ASSERT_EQ(static_cast<isize>(-1), w);
}

TEST(Ext4_FileIO, FsReadFileReturnsCorrectBytes,
     "fs->read_file() on hello.txt returns the known content") {
    WITH_EXT4(f);

    char buf[32] = {};
    i64 n = f.fs->read_file(ext4::EXT4_ROOT_INODE + 1,
                             /* try inode 3 for hello.txt – will vary, use API */
                             0, sizeof(buf), buf, false);
    // We can't guarantee inode 3 is hello.txt, so test via VFS path instead.
    // The raw-API test below uses the inode obtained via find.

    VfsNode* node = f.find_root("hello.txt");
    ASSERT_NOT_NULL(node);

    auto* nd = static_cast<Ext4Node*>(node->internal_data);
    u32 inode_no = nd->inode;
    Ext4Fixture::free_node(node);

    memset(buf, 0, sizeof(buf));
    i64 bytes = f.fs->read_file(inode_no, 0, sizeof(buf), buf, false);
    ASSERT_EQ(static_cast<i64>(HELLO_TXT_SIZE), bytes);
    ASSERT_MEM_EQ("hello from ext4\n", buf, HELLO_TXT_SIZE);
}

TEST(Ext4_FileIO, FsWriteFilePersistsAcrossRead,
     "fs->write_file() followed by fs->read_file() on same inode is consistent") {
    WITH_EXT4(f);

    VfsNode* node = f.find_root("hello.txt");
    ASSERT_NOT_NULL(node);
    auto* nd = static_cast<Ext4Node*>(node->internal_data);
    u32 inode_no = nd->inode;
    Ext4Fixture::free_node(node);

    constexpr char payload[] = "overwritten!";
    constexpr usize plen = sizeof(payload) - 1;

    i64 w = f.fs->write_file(inode_no, 0, plen, payload);
    ASSERT_EQ(static_cast<i64>(plen), w);

    char buf[32] = {};
    i64 r = f.fs->read_file(inode_no, 0, plen, buf, false);
    ASSERT_EQ(static_cast<i64>(plen), r);
    ASSERT_MEM_EQ(payload, buf, plen);
}

TEST(Ext4_FileIO, NodeSizeMatchesActualContent,
     "VfsNode::size equals the number of bytes readable from the file") {
    WITH_EXT4(f);
    EXT4_FIND(f, "binary.bin", node);
    NodeGuard guard(node);

    ASSERT_EQ(static_cast<usize>(BINARY_BIN_SIZE), node->size);
}

TEST(Ext4_FileIO, NodeSizeIsZeroForEmptyFile,
     "VfsNode::size is 0 for empty.txt") {
    WITH_EXT4(f);
    EXT4_FIND(f, "empty.txt", node);
    NodeGuard guard(node);

    ASSERT_EQ(static_cast<usize>(0), node->size);
}