// test_ext4_stat.cpp
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

static constexpr u16 MODE_TYPE_MASK = 0xF000u;
static constexpr u16 MODE_DIR       = 0x4000u;
static constexpr u16 MODE_REG       = 0x8000u;

TEST(Ext4_Stat, StatRootDirSucceeds,
     "stat() on the root directory returns 0") {
    WITH_EXT4(f);
    vespera_stat_t st = {};
    ASSERT_EQ(0, f.stat(f.root, &st));
}

TEST(Ext4_Stat, StatRegularFileSucceeds,
     "stat() on hello.txt returns 0") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);

    vespera_stat_t st = {};
    ASSERT_EQ(0, f.stat(node, &st));
}

TEST(Ext4_Stat, StatDirectorySucceeds,
     "stat() on subdir returns 0") {
    WITH_EXT4(f);
    EXT4_FIND(f, "subdir", node);
    NodeGuard guard(node);

    vespera_stat_t st = {};
    ASSERT_EQ(0, f.stat(node, &st));
}

TEST(Ext4_Stat, StatNullNodeFails,
     "stat() with null node returns a non-zero error code") {
    WITH_EXT4(f);
    vespera_stat_t st = {};
    ASSERT_NE(0, f.stat(nullptr, &st));
}

TEST(Ext4_Stat, StatNullOutFails,
     "stat() with null output buffer returns a non-zero error code") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);
    ASSERT_NE(0, f.stat(node, nullptr));
}

TEST(Ext4_Stat, StatHelloTxtSize,
     "stat() on hello.txt reports the correct file size") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);

    vespera_stat_t st = {};
    ASSERT_EQ(0, f.stat(node, &st));
    ASSERT_EQ(static_cast<u64>(HELLO_TXT_SIZE), st.size);
}

TEST(Ext4_Stat, StatEmptyFileSize,
     "stat() on empty.txt reports size 0") {
    WITH_EXT4(f);
    EXT4_FIND(f, "empty.txt", node);
    NodeGuard guard(node);

    vespera_stat_t st = {};
    ASSERT_EQ(0, f.stat(node, &st));
    ASSERT_EQ(static_cast<u64>(0), st.size);
}

TEST(Ext4_Stat, StatBinaryBinSize,
     "stat() on binary.bin reports 8192 bytes") {
    WITH_EXT4(f);
    EXT4_FIND(f, "binary.bin", node);
    NodeGuard guard(node);

    vespera_stat_t st = {};
    ASSERT_EQ(0, f.stat(node, &st));
    ASSERT_EQ(static_cast<u64>(BINARY_BIN_SIZE), st.size);
}

TEST(Ext4_Stat, StatBigBinSize,
     "stat() on big.bin reports 65536 bytes") {
    WITH_EXT4(f);
    EXT4_FIND(f, "big.bin", node);
    NodeGuard guard(node);

    vespera_stat_t st = {};
    ASSERT_EQ(0, f.stat(node, &st));
    ASSERT_EQ(static_cast<u64>(BIG_BIN_SIZE), st.size);
}

TEST(Ext4_Stat, StatRootDirModeIsDirectory,
     "stat() on root reports mode type == 0x4000 (directory)") {
    WITH_EXT4(f);
    vespera_stat_t st = {};
    ASSERT_EQ(0, f.stat(f.root, &st));
    ASSERT_EQ(static_cast<u32>(MODE_DIR),
              static_cast<u32>(st.mode & MODE_TYPE_MASK));
}

TEST(Ext4_Stat, StatSubdirModeIsDirectory,
     "stat() on subdir reports mode type == 0x4000 (directory)") {
    WITH_EXT4(f);
    EXT4_FIND(f, "subdir", node);
    NodeGuard guard(node);

    vespera_stat_t st = {};
    ASSERT_EQ(0, f.stat(node, &st));
    ASSERT_EQ(static_cast<u32>(MODE_DIR),
              static_cast<u32>(st.mode & MODE_TYPE_MASK));
}

TEST(Ext4_Stat, StatRegularFileModeIsFile,
     "stat() on hello.txt reports mode type == 0x8000 (regular file)") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);

    vespera_stat_t st = {};
    ASSERT_EQ(0, f.stat(node, &st));
    ASSERT_EQ(static_cast<u32>(MODE_REG),
              static_cast<u32>(st.mode & MODE_TYPE_MASK));
}

TEST(Ext4_Stat, StatBinaryFileModeIsFile,
     "stat() on binary.bin reports mode type == 0x8000 (regular file)") {
    WITH_EXT4(f);
    EXT4_FIND(f, "binary.bin", node);
    NodeGuard guard(node);

    vespera_stat_t st = {};
    ASSERT_EQ(0, f.stat(node, &st));
    ASSERT_EQ(static_cast<u32>(MODE_REG),
              static_cast<u32>(st.mode & MODE_TYPE_MASK));
}

TEST(Ext4_Stat, StatRootInodeIsTwo,
     "stat() on the root directory reports inode 2") {
    WITH_EXT4(f);
    vespera_stat_t st = {};
    ASSERT_EQ(0, f.stat(f.root, &st));
    ASSERT_EQ(static_cast<u64>(ext4::EXT4_ROOT_INODE), st.inode_id);
}

TEST(Ext4_Stat, StatInodeDiffersAcrossFiles,
     "stat() reports different inode numbers for different files") {
    WITH_EXT4(f);

    EXT4_FIND(f, "hello.txt",   n1);
    EXT4_FIND(f, "binary.bin",  n2);
    NodeGuard g1(n1), g2(n2);

    vespera_stat_t s1 = {}, s2 = {};
    ASSERT_EQ(0, f.stat(n1, &s1));
    ASSERT_EQ(0, f.stat(n2, &s2));
    ASSERT_NE(s1.inode_id, s2.inode_id);
}

TEST(Ext4_Stat, StatFileDiffersFromDirectory,
     "stat() gives different inodes for a file and a directory") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", file);
    EXT4_FIND(f, "subdir",    dir);
    NodeGuard gf(file), gd(dir);

    vespera_stat_t sf = {}, sd = {};
    ASSERT_EQ(0, f.stat(file, &sf));
    ASSERT_EQ(0, f.stat(dir,  &sd));
    ASSERT_NE(sf.inode_id, sd.inode_id);
}

TEST(Ext4_Stat, StatSizeMatchesNodeSize,
     "size from stat() matches VfsNode::size for hello.txt") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", node);
    NodeGuard guard(node);

    vespera_stat_t st = {};
    ASSERT_EQ(0, f.stat(node, &st));
    ASSERT_EQ(static_cast<u64>(node->size), st.size);
}

TEST(Ext4_Stat, StatSizeMatchesNodeSizeBigFile,
     "size from stat() matches VfsNode::size for big.bin") {
    WITH_EXT4(f);
    EXT4_FIND(f, "big.bin", node);
    NodeGuard guard(node);

    vespera_stat_t st = {};
    ASSERT_EQ(0, f.stat(node, &st));
    ASSERT_EQ(static_cast<u64>(node->size), st.size);
}

TEST(Ext4_Stat, StatAfterCreateReflectsNewFile,
     "stat() on a newly created file returns size 0 and correct type") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "created_stat.txt"));

    VfsNode* node = f.find_root("created_stat.txt");
    ASSERT_NOT_NULL(node);
    NodeGuard guard(node);

    vespera_stat_t st = {};
    ASSERT_EQ(0, f.stat(node, &st));
    ASSERT_EQ(static_cast<u64>(0), st.size);
    ASSERT_EQ(static_cast<u32>(MODE_REG),
              static_cast<u32>(st.mode & MODE_TYPE_MASK));
}

TEST(Ext4_Stat, FsStatRootInode,
     "fs->stat() on EXT4_ROOT_INODE succeeds and reports directory mode") {
    WITH_EXT4(f);

    vespera_stat_t st = {};
    ASSERT_TRUE(f.fs->stat(ext4::EXT4_ROOT_INODE, &st, 0));
    ASSERT_EQ(static_cast<u64>(ext4::EXT4_ROOT_INODE), st.inode_id);
    ASSERT_EQ(static_cast<u32>(MODE_DIR),
              static_cast<u32>(st.mode & MODE_TYPE_MASK));
}