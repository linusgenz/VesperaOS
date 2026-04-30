// test_ext4_fs.cpp
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

TEST(Ext4_FS, ValidImageIsValid, "FileSystem mounted from test_ext4.img reports is_valid() == true") {
    WITH_EXT4(f);
    ASSERT_TRUE(f.fs->is_valid());
}

TEST(Ext4_FS, InvalidAllZeroImageIsNotValid, "FileSystem on a zeroed block device is not valid") {
    auto* mdev = new MockBlockDevice(256);
    // MockBlockDevice is already zero-filled on construction.
    ext4::FileSystem bad_fs(mdev);
    ASSERT_FALSE(bad_fs.is_valid());
    delete mdev;
}

TEST(Ext4_FS, InvalidAllFFImageIsNotValid, "FileSystem on a 0xFF-filled block device is not valid") {
    auto* mdev = new MockBlockDevice(256);
    memset(mdev->raw(), 0xFF, 256 * MockBlockDevice::SECTOR_SIZE);
    ext4::FileSystem bad_fs(mdev);
    ASSERT_FALSE(bad_fs.is_valid());
    delete mdev;
}

TEST(Ext4_FS, InvalidSmallDeviceIsNotValid, "A device too small to hold even a superblock is not valid") {
    // 1 sector = 512 bytes — superblock is at offset 1024, so can't exist.
    auto* mdev = new MockBlockDevice(1);
    ext4::FileSystem bad_fs(mdev);
    ASSERT_FALSE(bad_fs.is_valid());
    delete mdev;
}

TEST(Ext4_FS, SuperblockMagicIsCorrect, "get_superblock()->s_magic == EXT4_MAGIC (0xEF53)") {
    WITH_EXT4(f);
    ASSERT_EQ(static_cast<u16>(ext4::EXT4_MAGIC), f.fs->get_superblock()->s_magic);
}

TEST(Ext4_FS, SuperblockLabelIsVespTest, "Filesystem volume label matches the one set by mkfs.ext4") {
    WITH_EXT4(f);
    // The label is "vesp_test" (9 bytes); s_volume_name is 16 bytes.
    ASSERT_EQ(0, strncmp(reinterpret_cast<const char*>(f.fs->get_superblock()->s_volume_name), "vesp_test", 9));
}

TEST(Ext4_FS, SuperblockInodeCountIsPositive, "s_inodes_count is greater than zero") {
    WITH_EXT4(f);
    ASSERT_TRUE(f.fs->get_superblock()->s_inodes_count > 0u);
}

TEST(Ext4_FS, SuperblockBlockCountIsPositive, "s_blocks_count_lo is greater than zero") {
    WITH_EXT4(f);
    ASSERT_TRUE(f.fs->get_superblock()->s_blocks_count_lo > 0u);
}

TEST(Ext4_FS, BlockSizeIs4096, "get_block_size() returns 4096 (mkfs -b 4096)") {
    WITH_EXT4(f);
    ASSERT_EQ(static_cast<u32>(4096), f.fs->get_block_size());
}

TEST(Ext4_FS, BlockSizeIsPowerOfTwo, "Block size is a power of two and at least 1024") {
    WITH_EXT4(f);
    u32 bs = f.fs->get_block_size();
    ASSERT_GE(bs, static_cast<u32>(1024));
    ASSERT_EQ(static_cast<u32>(0), bs & (bs - 1));
}

TEST(Ext4_FS, ProbeDetectsExt4, "ext4_driver.probe() returns 1 for a valid ext4 image") {
    WITH_EXT4(f);
    // Re-probe with the same device (already loaded into MockBlockDevice).
    FilesystemInfo info = {};
    int result = ext4_driver.probe(f.dev, &info);
    ASSERT_EQ(1, result);
}

TEST(Ext4_FS, ProbeExtractsLabel, "ext4_driver.probe() copies the volume name into FilesystemInfo::label") {
    WITH_EXT4(f);
    FilesystemInfo info = {};
    ext4_driver.probe(f.dev, &info);
    ASSERT_EQ(0, strncmp(info.label, "vesp_test", 9));
}

TEST(Ext4_FS, ProbeRejectsZeroedDevice, "ext4_driver.probe() returns 0 for a zeroed device") {
    auto* mdev = new MockBlockDevice(256);
    FilesystemInfo info = {};
    ASSERT_EQ(0, ext4_driver.probe(mdev, &info));
    delete mdev;
}

TEST(Ext4_FS, RootInodeIsTwo, "The internal_data of the root VfsNode has inode == EXT4_ROOT_INODE (2)") {
    WITH_EXT4(f);
    auto* nd = static_cast<Ext4Node*>(f.root->internal_data);
    ASSERT_EQ(static_cast<u32>(ext4::EXT4_ROOT_INODE), nd->inode);
}

TEST(Ext4_FS, RootNodeIsDirectory, "The root VfsNode type is Directory") {
    WITH_EXT4(f);
    ASSERT_EQ(static_cast<int>(VfsNodeType::Directory), static_cast<int>(f.root->type));
}

TEST(Ext4_FS, RootNodeIsPermanent, "The root VfsNode has permanent == true") {
    WITH_EXT4(f);
    ASSERT_TRUE(f.root->permanent);
}

TEST(Ext4_FS, ReadDirectoryRootHasExpectedEntries, "Root directory contains at least the known image files") {
    WITH_EXT4(f);

    usize count = 0;
    auto res = f.fs->read_directory(ext4::EXT4_ROOT_INODE, count);
    ASSERT_TRUE(res.is_ok());

    ext4::FileEntry* entries = res.value();

    ASSERT_GE(count, static_cast<usize>(9));

    kernel::memory::free(entries);
}

TEST(Ext4_FS, ReadDirectoryListMatchesFindPath, "Every name returned by read_directory() can be found via find()") {
    WITH_EXT4(f);

    usize count = 0;
    auto res = f.fs->read_directory(ext4::EXT4_ROOT_INODE, count);
    ASSERT_TRUE(res.is_ok());

    ext4::FileEntry* entries = res.value();

    for (usize i = 0; i < count; ++i) {
        const char* name = entries[i].get_name();

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

        VfsNode* found = f.find_root(name);
        ASSERT_NOT_NULL(found);
        Ext4Fixture::free_node(found);
    }

    kernel::memory::free(entries);
}

TEST(Ext4_FS, FindDeepDirectoryLevel1, "find() can locate the 'deep' directory in root") {
    WITH_EXT4(f);
    EXT4_FIND(f, "deep", deep);
    NodeGuard guard(deep);

    ASSERT_EQ(static_cast<int>(VfsNodeType::Directory), static_cast<int>(deep->type));
}

TEST(Ext4_FS, FindDeepDirectoryLevel2, "find() can locate 'deep/level1' by chaining two find() calls") {
    WITH_EXT4(f);
    EXT4_FIND(f, "deep", deep);

    VfsNode* level1 = f.find(deep, "level1");
    ASSERT_NOT_NULL(level1);
    ASSERT_EQ(static_cast<int>(VfsNodeType::Directory), static_cast<int>(level1->type));

    Ext4Fixture::free_node(level1);
    Ext4Fixture::free_node(deep);
}

TEST(Ext4_FS, FindDeepLeafFile, "find() can locate 'deep/level1/leaf.txt' via chained finds") {
    WITH_EXT4(f);

    EXT4_FIND(f, "deep", deep);
    VfsNode* level1 = f.find(deep, "level1");
    ASSERT_NOT_NULL(level1);
    VfsNode* leaf = f.find(level1, "leaf.txt");
    ASSERT_NOT_NULL(leaf);

    ASSERT_EQ(static_cast<int>(VfsNodeType::File), static_cast<int>(leaf->type));

    char buf[16] = {};
    isize r = f.read(leaf, 0, sizeof(buf), buf);
    ASSERT_EQ(static_cast<isize>(LEAF_TXT_SIZE), r);
    ASSERT_MEM_EQ("deep leaf\n", buf, LEAF_TXT_SIZE);

    Ext4Fixture::free_node(leaf);
    Ext4Fixture::free_node(level1);
    Ext4Fixture::free_node(deep);
}

TEST(Ext4_FS, StressCreateWriteReadDelete10Files, "Create, write, read, and delete 10 files without errors") {
    WITH_EXT4(f);

    constexpr int N = 10;
    char names[N][24];
    for (int i = 0; i < N; ++i) snprintf(names[i], sizeof(names[i]), "stress_%02d.txt", i);

    // Create
    for (int i = 0; i < N; ++i) ASSERT_EQ(0, f.create(f.root, names[i]));

    // Write
    for (int i = 0; i < N; ++i) {
        VfsNode* node = f.find_root(names[i]);
        ASSERT_NOT_NULL(node);
        char data[32];
        snprintf(data, sizeof(data), "data for %d", i);
        ASSERT_GE(f.write(node, 0, strlen(data), data), static_cast<isize>(1));
        Ext4Fixture::free_node(node);
    }

    // Read and verify
    for (int i = 0; i < N; ++i) {
        VfsNode* node = f.find_root(names[i]);
        ASSERT_NOT_NULL(node);
        char expected[32], got[32] = {};
        snprintf(expected, sizeof(expected), "data for %d", i);
        usize len = strlen(expected);
        isize r = f.read(node, 0, len, got);
        ASSERT_EQ(static_cast<isize>(len), r);
        ASSERT_MEM_EQ(expected, got, len);
        Ext4Fixture::free_node(node);
    }

    // Delete
    for (int i = 0; i < N; ++i) ASSERT_EQ(0, f.unlink(f.root, names[i]));

    // Verify absence
    auto listing = f.list_root();
    for (int i = 0; i < N; ++i) ASSERT_FALSE(f.list_contains(listing, names[i]));
}

TEST(Ext4_FS, StressCreateManyFilesInSubdir, "Creating 30 files inside a subdirectory all appear in the listing") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.mkdir(f.root, "mass_dir"));

    VfsNode* dir = f.find_root("mass_dir");
    ASSERT_NOT_NULL(dir);
    NodeGuard guard(dir);

    constexpr int N = 30;
    char name[24];
    for (int i = 0; i < N; ++i) {
        snprintf(name, sizeof(name), "f%03d.txt", i);
        ASSERT_EQ(0, f.create(dir, name));
    }

    auto names = f.list(dir);
    ASSERT_GE(names.size(), static_cast<usize>(N));

    for (int i = 0; i < N; ++i) {
        snprintf(name, sizeof(name), "f%03d.txt", i);
        ASSERT_TRUE(f.list_contains(names, name));
    }
}

TEST(Ext4_FS, StressMixedOperations, "Mix of create, write, read, rename, unlink is consistent") {
    WITH_EXT4(f);

    // Phase 1: create 5 files and write unique content
    for (int i = 0; i < 5; ++i) {
        char name[24];
        snprintf(name, sizeof(name), "mix_%d.txt", i);
        ASSERT_EQ(0, f.create(f.root, name));

        VfsNode* node = f.find_root(name);
        ASSERT_NOT_NULL(node);
        char data[16];
        snprintf(data, sizeof(data), "value=%d", i);
        ASSERT_GE(f.write(node, 0, strlen(data), data), static_cast<isize>(1));
        Ext4Fixture::free_node(node);
    }

    // Phase 2: rename even-indexed files
    for (int i = 0; i < 5; i += 2) {
        char old_name[24], new_name[24];
        snprintf(old_name, sizeof(old_name), "mix_%d.txt", i);
        snprintf(new_name, sizeof(new_name), "renamed_%d.txt", i);
        ASSERT_EQ(0, f.rename(f.root, old_name, f.root, new_name));
    }

    // Phase 3: verify odd-indexed files still readable
    for (int i = 1; i < 5; i += 2) {
        char name[24];
        snprintf(name, sizeof(name), "mix_%d.txt", i);
        VfsNode* node = f.find_root(name);
        ASSERT_NOT_NULL(node);

        char expected[16], got[16] = {};
        snprintf(expected, sizeof(expected), "value=%d", i);
        usize len = strlen(expected);
        isize r = f.read(node, 0, len, got);
        ASSERT_EQ(static_cast<isize>(len), r);
        ASSERT_MEM_EQ(expected, got, len);
        Ext4Fixture::free_node(node);
    }

    // Phase 4: unlink all
    for (int i = 0; i < 5; i += 2) {
        char new_name[24];
        snprintf(new_name, sizeof(new_name), "renamed_%d.txt", i);
        ASSERT_EQ(0, f.unlink(f.root, new_name));
    }
    for (int i = 1; i < 5; i += 2) {
        char name[24];
        snprintf(name, sizeof(name), "mix_%d.txt", i);
        ASSERT_EQ(0, f.unlink(f.root, name));
    }
}

TEST(Ext4_FS, StressMultiBlockWriteReadCycles, "5 cycles of write-8192-bytes + full-read produce consistent results") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "cycled_big.bin"));

    VfsNode* node = f.find_root("cycled_big.bin");
    ASSERT_NOT_NULL(node);
    NodeGuard guard(node);

    for (int cycle = 0; cycle < 5; ++cycle) {
        std::vector<uint8_t> src(8192);
        for (usize i = 0; i < 8192; ++i) src[i] = static_cast<uint8_t>((i + cycle) & 0xFF);

        ASSERT_EQ(0, f.truncate(node, 0));
        isize w = f.write(node, 0, 8192, src.data());
        ASSERT_EQ(static_cast<isize>(8192), w);

        std::vector<uint8_t> dst(8192, 0);
        isize r = f.read(node, 0, 8192, dst.data());
        ASSERT_EQ(static_cast<isize>(8192), r);
        ASSERT_MEM_EQ(src.data(), dst.data(), 8192);
    }
}