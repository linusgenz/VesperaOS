// test_fat32_fileio.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 02.03.26.
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

// =============================================================================
// FAT32 Tests — File I/O
// =============================================================================
// Covers: WriteFile / ReadFile
//   - Basic write and read
//   - Partial overwrite (offset in the middle)
//   - Offset beyond EOF (must be rejected)
//   - Multi-cluster files
//   - Exact cluster boundary and boundary +1
//   - All byte values 0x00–0xFF
//   - Sequential append simulation
//   - fileSize persistence after write
//   - Read clamped to fileSize
//   - Read with non-zero offset
//   - Overwrite with same size
//   - Read-Only / System attribute protection
//   - Large file (>64 KB)
// =============================================================================

#include "fat32_fixture.h"

TEST(FAT32_FileIO, BasicWriteRead, "Write a string and read it back") {
    WITH_FAT32(f);
    auto node = f.create_file("BASIC.TXT");

    constexpr char data[] = "Hello, VesperaOS!";
    const size_t len = strlen(data);

    ASSERT_TRUE(f.write(node, data, len));
    ASSERT_EQ(len, (size_t)node.file_size);

    auto result = f.read(node, len);
    ASSERT_EQ(len, result.size());
    ASSERT_MEM_EQ(data, result.data(), len);
}

TEST(FAT32_FileIO, WriteEmptyRejected, "Write with len=0 or null buffer is rejected") {
    WITH_FAT32(f);
    auto node = f.create_file("EMPTY.TXT");
    ASSERT_FALSE(f.fs->write_file(&node, "X", 0, 0));
    ASSERT_FALSE(f.fs->write_file(&node, nullptr, 5, 0));
}

TEST(FAT32_FileIO, ReadEmptyFile, "Reading an empty file returns 0 bytes") {
    WITH_FAT32(f);
    auto node = f.create_file("EMPTYRD.TXT");

    char buf[16] = {};
    Result<usize> r = f.fs->read_file(&node, buf, sizeof(buf), 0);
    ASSERT_TRUE(r.is_ok());
    ASSERT_EQ(static_cast<size_t>(0), r.unwrap());
}

TEST(FAT32_FileIO, ReadExactSize, "Read exactly fileSize bytes") {
    WITH_FAT32(f);
    auto node = f.create_file("EXACT.TXT");

    constexpr char data[] = "ABCDE";
    ASSERT_TRUE(f.write(node, data, 5));

    char buf[8] = {};
    Result<usize> r =f.fs->read_file(&node, buf, 5, 0);
    ASSERT_TRUE(r.is_ok());
    ASSERT_EQ(static_cast<size_t>(5), r.unwrap());
    ASSERT_MEM_EQ(data, buf, 5);
}

TEST(FAT32_FileIO, ReadOffsetAtEOF, "Read with offset >= fileSize returns 0 bytes") {
    WITH_FAT32(f);
    auto node = f.create_file("OFFEOF.TXT");
    ASSERT_TRUE(f.write(node, "12345", 5));

    char buf[8];

    // offset == fileSize
    Result<usize> r = f.fs->read_file(&node, buf, 8, 5);
    ASSERT_TRUE(r.is_ok());
    ASSERT_EQ(static_cast<size_t>(0), r.unwrap());

    // offset far past EOF
    Result<usize> rr = f.fs->read_file(&node, buf, 8,  100);
    ASSERT_TRUE(rr.is_ok());
    ASSERT_EQ(static_cast<size_t>(0), rr.unwrap());
}

TEST(FAT32_FileIO, PartialOverwrite, "Overwrite bytes in the middle of a file") {
    WITH_FAT32(f);
    auto node = f.create_file("PARTIAL.TXT");

    ASSERT_TRUE(f.write(node, "AAAAAAAAAA", 10));
    ASSERT_TRUE(f.write(node, "BBB", 3, 3));

    auto result = f.read(node, 10);
    ASSERT_EQ(static_cast<size_t>(10), result.size());
    ASSERT_MEM_EQ("AAABBBAAAA", result.data(), 10);
}

TEST(FAT32_FileIO, WriteOffsetBeyondEOF, "Write with offset > fileSize is rejected") {
    WITH_FAT32(f);
    auto node = f.create_file("NOHOLE.TXT");
    ASSERT_TRUE(f.write(node, "12345", 5));
    ASSERT_FALSE(f.write(node, "X", 1, 9999));
}

TEST(FAT32_FileIO, MultiClusterWrite, "Write and read a file larger than one cluster") {
    WITH_FAT32(f);
    auto node = f.create_file("MULTI.TXT");

    constexpr size_t sz = 8192;
    std::vector<uint8_t> src(sz, 0xAB);

    ASSERT_TRUE(f.write(node, src.data(), sz));

    auto result = f.read(node, sz);
    ASSERT_EQ(sz, result.size());
    ASSERT_MEM_EQ(src.data(), result.data(), sz);
}

TEST(FAT32_FileIO, ExactlyTwoClusters, "Write and read a file that spans exactly two clusters") {
    WITH_FAT32(f);
    auto node = f.create_file("2CLUST.TXT");

    constexpr size_t sz = static_cast<size_t>(2) * 4096;
    std::vector<uint8_t> src(sz);
    for (size_t i = 0; i < sz; i++) src[i] = static_cast<uint8_t>(i & 0xFF);

    ASSERT_TRUE(f.write(node, src.data(), sz));

    auto result = f.read(node, sz);
    ASSERT_EQ(sz, result.size());
    ASSERT_MEM_EQ(src.data(), result.data(), sz);
}

TEST(FAT32_FileIO, ClusterBoundaryPlusOne, "Write cluster-size + 1 byte (crosses boundary)") {
    WITH_FAT32(f);
    auto node = f.create_file("BORDER.TXT");

    constexpr size_t sz = 4096 + 1;
    std::vector<uint8_t> src(sz, 0x7E);

    ASSERT_TRUE(f.write(node, src.data(), sz));

    auto result = f.read(node, sz);
    ASSERT_EQ(sz, result.size());
    ASSERT_MEM_EQ(src.data(), result.data(), sz);
}

TEST(FAT32_FileIO, AllByteValues, "Write and read all byte values 0x00–0xFF") {
    WITH_FAT32(f);
    auto node = f.create_file("BINDATA.BIN");

    uint8_t all[256];
    for (int i = 0; i < 256; i++) all[i] = static_cast<uint8_t>(i);

    ASSERT_TRUE(f.write(node, all, 256));

    auto result = f.read(node, 256);
    ASSERT_EQ(static_cast<size_t>(256), result.size());
    ASSERT_MEM_EQ(all, result.data(), 256);
}

TEST(FAT32_FileIO, SequentialAppend, "Simulate append by writing at fileSize offset") {
    WITH_FAT32(f);
    auto node = f.create_file("APPEND.TXT");

    ASSERT_TRUE(f.write(node, "Hello", 5, 0));
    ASSERT_EQ(static_cast<size_t>(5), (size_t)node.file_size);

    ASSERT_TRUE(f.write(node, " World", 6, 5));
    ASSERT_EQ(static_cast<size_t>(11), (size_t)node.file_size);

    auto result = f.read(node, 11);
    ASSERT_EQ(static_cast<size_t>(11), result.size());
    ASSERT_MEM_EQ("Hello World", result.data(), 11);
}

TEST(FAT32_FileIO, FileSizePersisted, "fileSize is correct after reloading the node from disk") {
    WITH_FAT32(f);
    auto node = f.create_file("PERSIST.TXT");
    ASSERT_TRUE(f.write(node, "TESTDATA", 8));

    auto fresh = f.find_file_node("PERSIST.TXT");
    ASSERT_EQ(static_cast<size_t>(8), (size_t)fresh.file_size);
}

TEST(FAT32_FileIO, ReadClampedToFileSize, "Read with len > fileSize reads only fileSize bytes") {
    WITH_FAT32(f);
    auto node = f.create_file("CLAMP.TXT");
    ASSERT_TRUE(f.write(node, "ABC", 3));

    char buf[64] = {};
    Result<usize> r = f.fs->read_file(&node, buf, 64, 0);
    ASSERT_TRUE(r.is_ok());
    ASSERT_EQ(static_cast<size_t>(3), r.unwrap());
}

TEST(FAT32_FileIO, OverwriteSameSize, "Overwriting with same size replaces content") {
    WITH_FAT32(f);
    auto node = f.create_file("OVERWR.TXT");
    ASSERT_TRUE(f.write(node, "AAAAAA", 6));
    ASSERT_TRUE(f.write(node, "BBBBBB", 6, 0));

    auto result = f.read(node, 6);
    ASSERT_EQ(static_cast<size_t>(6), result.size());
    ASSERT_MEM_EQ("BBBBBB", result.data(), 6);
}

TEST(FAT32_FileIO, ReadAtOffset, "Read with non-zero offset returns correct bytes") {
    WITH_FAT32(f);
    auto node = f.create_file("OFFRD.TXT");
    ASSERT_TRUE(f.write(node, "0123456789", 10));

    char buf[8] = {};
    size_t actual = 0;
    Result<usize> r = f.fs->read_file(&node, buf, 4, 3);
    ASSERT_TRUE(r.is_ok());
    ASSERT_EQ(static_cast<size_t>(4), r.unwrap());
    ASSERT_MEM_EQ("3456", buf, 4);
}

TEST(FAT32_FileIO, ReadOnlyProtection, "Write to a read-only file is rejected") {
    WITH_FAT32(f);
    auto node = f.create_file("RDONLY.TXT");
    ASSERT_TRUE(f.write(node, "data", 4));

    node.dir_entry.attr |= fat32::ATTR_READ_ONLY;
    ASSERT_FALSE(f.write(node, "nope", 4));
}

TEST(FAT32_FileIO, SystemFileProtection, "Write to a system file is rejected") {
    WITH_FAT32(f);
    auto node = f.create_file("SYSFILE.TXT");
    node.dir_entry.attr |= fat32::ATTR_SYSTEM;
    ASSERT_FALSE(f.write(node, "X", 1));
}

TEST(FAT32_FileIO, LargeFile128KB, "Write and read a 128 KB file") {
    WITH_FAT32(f);
    auto node = f.create_file("LARGE.BIN");

    constexpr size_t sz = static_cast<size_t>(128) * 1024;
    std::vector<uint8_t> src(sz);
    for (size_t i = 0; i < sz; i++) src[i] = static_cast<uint8_t>(i * 7 + 13);

    ASSERT_TRUE(f.write(node, src.data(), sz));
    ASSERT_EQ(sz, (size_t)node.file_size);

    auto result = f.read(node, sz);
    ASSERT_EQ(sz, result.size());
    ASSERT_MEM_EQ(src.data(), result.data(), sz);
}