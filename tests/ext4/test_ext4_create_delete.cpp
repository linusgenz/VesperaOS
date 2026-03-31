// test_ext4_create_delete.cpp
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

TEST(Ext4_Create, CreateFileInRoot,
     "create() in root directory succeeds and returns 0") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "newfile.txt"));
}

TEST(Ext4_Create, CreateFileAppearsInListing,
     "Newly created file appears in readdir listing") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "appear.txt"));

    auto names = f.list_root();
    ASSERT_TRUE(f.list_contains(names, "appear.txt"));
}

TEST(Ext4_Create, CreateFileIsRegularNode,
     "find() on a created file returns a non-directory VfsNode") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "regfile.txt"));

    VfsNode* node = f.find_root("regfile.txt");
    ASSERT_NOT_NULL(node);
    ASSERT_EQ(static_cast<int>(VfsNodeType::File),
              static_cast<int>(node->type));
    Ext4Fixture::free_node(node);
}

TEST(Ext4_Create, CreateFileInitialSizeZero,
     "A newly created file has size 0") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "zero.txt"));

    VfsNode* node = f.find_root("zero.txt");
    ASSERT_NOT_NULL(node);
    ASSERT_EQ(static_cast<usize>(0), node->size);
    Ext4Fixture::free_node(node);
}

TEST(Ext4_Create, CreateFileInSubdir,
     "create() inside a subdirectory succeeds and is visible there") {
    WITH_EXT4(f);
    EXT4_FIND(f, "subdir", subdir);
    NodeGuard guard(subdir);

    ASSERT_EQ(0, f.create(subdir, "subfile.txt"));

    auto names = f.list(subdir);
    ASSERT_TRUE(f.list_contains(names, "subfile.txt"));
}

TEST(Ext4_Create, CreateFileWriteAndRead,
     "After creation, the file can be written and read back") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "wr.txt"));

    VfsNode* node = f.find_root("wr.txt");
    ASSERT_NOT_NULL(node);
    NodeGuard guard(node);

    constexpr char data[] = "hello ext4 write";
    constexpr usize len   = sizeof(data) - 1;

    isize w = f.write(node, 0, len, data);
    ASSERT_EQ(static_cast<isize>(len), w);

    char buf[64] = {};
    isize r = f.read(node, 0, len, buf);
    ASSERT_EQ(static_cast<isize>(len), r);
    ASSERT_MEM_EQ(data, buf, len);
}

TEST(Ext4_Create, CreateMultipleFiles,
     "Creating 10 files in root all appear in the listing") {
    WITH_EXT4(f);

    char name[32];
    for (int i = 0; i < 10; ++i) {
        snprintf(name, sizeof(name), "file_%02d.txt", i);
        ASSERT_EQ(0, f.create(f.root, name));
    }

    auto names = f.list_root();
    for (int i = 0; i < 10; ++i) {
        snprintf(name, sizeof(name), "file_%02d.txt", i);
        ASSERT_TRUE(f.list_contains(names, name));
    }
}

TEST(Ext4_Create, CreateFileNullParent,
     "create() with null parent returns non-zero (error)") {
    WITH_EXT4(f);
    ASSERT_NE(0, f.create(nullptr, "x.txt"));
}

TEST(Ext4_Create, CreateFileNullName,
     "create() with null name returns non-zero (error)") {
    WITH_EXT4(f);
    ASSERT_NE(0, f.create(f.root, nullptr));
}

TEST(Ext4_Create, CreateFileOnFileNodeFails,
     "create() with a file node as parent returns an error") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", file);
    NodeGuard guard(file);

    ASSERT_NE(0, f.create(file, "child.txt"));
}

TEST(Ext4_Unlink, UnlinkRemovesFile,
     "unlink() removes a file from the directory listing") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "del.txt"));
    ASSERT_EQ(0, f.unlink(f.root, "del.txt"));

    auto names = f.list_root();
    ASSERT_FALSE(f.list_contains(names, "del.txt"));
}

TEST(Ext4_Unlink, UnlinkMakesFindReturnNull,
     "After unlink(), find() can no longer locate the file") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.create(f.root, "gone.txt"));
    ASSERT_EQ(0, f.unlink(f.root, "gone.txt"));

    VfsNode* node = f.find_root("gone.txt");
    ASSERT_NULL(node);
}

TEST(Ext4_Unlink, UnlinkMissingFileReturnsError,
     "unlink() on a non-existent file returns non-zero") {
    WITH_EXT4(f);
    ASSERT_NE(0, f.unlink(f.root, "does_not_exist.txt"));
}

TEST(Ext4_Unlink, UnlinkNullParent,
     "unlink() with null parent returns non-zero") {
    WITH_EXT4(f);
    ASSERT_NE(0, f.unlink(nullptr, "hello.txt"));
}

TEST(Ext4_Unlink, UnlinkNullName,
     "unlink() with null name returns non-zero") {
    WITH_EXT4(f);
    ASSERT_NE(0, f.unlink(f.root, nullptr));
}

TEST(Ext4_Unlink, UnlinkExistingImageFile,
     "unlink() can remove a file that was part of the original image") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.unlink(f.root, "hello.txt"));

    auto names = f.list_root();
    ASSERT_FALSE(f.list_contains(names, "hello.txt"));
}

TEST(Ext4_Unlink, UnlinkFromSubdir,
     "unlink() removes a file from a subdirectory") {
    WITH_EXT4(f);
    EXT4_FIND(f, "subdir", subdir);
    NodeGuard guard(subdir);

    ASSERT_EQ(0, f.create(subdir, "todel.txt"));
    ASSERT_EQ(0, f.unlink(subdir, "todel.txt"));

    auto names = f.list(subdir);
    ASSERT_FALSE(f.list_contains(names, "todel.txt"));
}

TEST(Ext4_Unlink, CreateWriteUnlinkCycle,
     "Create → write → unlink cycle repeatable without corruption") {
    WITH_EXT4(f);

    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(0, f.create(f.root, "cycle.txt"));

        VfsNode* node = f.find_root("cycle.txt");
        ASSERT_NOT_NULL(node);

        char data[16];
        snprintf(data, sizeof(data), "cycle %d", i);
        ASSERT_GE(f.write(node, 0, strlen(data), data), static_cast<isize>(1));
        Ext4Fixture::free_node(node);

        ASSERT_EQ(0, f.unlink(f.root, "cycle.txt"));
    }

    auto names = f.list_root();
    ASSERT_FALSE(f.list_contains(names, "cycle.txt"));
}

TEST(Ext4_Mkdir, MkdirInRoot,
     "mkdir() in root directory succeeds and returns 0") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.mkdir(f.root, "newdir"));
}

TEST(Ext4_Mkdir, MkdirAppearsInListing,
     "Newly created directory appears in readdir listing") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.mkdir(f.root, "listing_dir"));

    auto names = f.list_root();
    ASSERT_TRUE(f.list_contains(names, "listing_dir"));
}

TEST(Ext4_Mkdir, MkdirNodeIsDirectory,
     "find() on a created directory returns a VfsNodeType::Directory node") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.mkdir(f.root, "isdir"));

    VfsNode* node = f.find_root("isdir");
    ASSERT_NOT_NULL(node);
    ASSERT_EQ(static_cast<int>(VfsNodeType::Directory),
              static_cast<int>(node->type));
    Ext4Fixture::free_node(node);
}

TEST(Ext4_Mkdir, MkdirHasDotEntries,
     "A newly created directory contains '.' and '..' entries") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.mkdir(f.root, "dotdir"));

    VfsNode* dir = f.find_root("dotdir");
    ASSERT_NOT_NULL(dir);
    NodeGuard guard(dir);

    auto names = f.list(dir);
    ASSERT_TRUE(f.list_contains(names, "."));
    ASSERT_TRUE(f.list_contains(names, ".."));
}

TEST(Ext4_Mkdir, MkdirNullParent,
     "mkdir() with null parent returns non-zero") {
    WITH_EXT4(f);
    ASSERT_NE(0, f.mkdir(nullptr, "dir"));
}

TEST(Ext4_Mkdir, MkdirNullName,
     "mkdir() with null name returns non-zero") {
    WITH_EXT4(f);
    ASSERT_NE(0, f.mkdir(f.root, nullptr));
}

TEST(Ext4_Mkdir, MkdirOnFileNodeFails,
     "mkdir() with a regular file as parent returns an error") {
    WITH_EXT4(f);
    EXT4_FIND(f, "hello.txt", file);
    NodeGuard guard(file);

    ASSERT_NE(0, f.mkdir(file, "child"));
}

TEST(Ext4_Mkdir, MkdirThenCreateFileInside,
     "A file can be created inside a freshly made directory") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.mkdir(f.root, "container"));

    VfsNode* dir = f.find_root("container");
    ASSERT_NOT_NULL(dir);
    NodeGuard guard(dir);

    ASSERT_EQ(0, f.create(dir, "inside.txt"));
    auto names = f.list(dir);
    ASSERT_TRUE(f.list_contains(names, "inside.txt"));
}

TEST(Ext4_Rmdir, RmdirEmptyDir,
     "rmdir() removes an empty directory and returns 0") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.mkdir(f.root, "emptydir"));
    ASSERT_EQ(0, f.rmdir(f.root, "emptydir"));

    auto names = f.list_root();
    ASSERT_FALSE(f.list_contains(names, "emptydir"));
}

TEST(Ext4_Rmdir, RmdirMakesFindReturnNull,
     "After rmdir(), find() can no longer locate the directory") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.mkdir(f.root, "gone_dir"));
    ASSERT_EQ(0, f.rmdir(f.root, "gone_dir"));

    VfsNode* node = f.find_root("gone_dir");
    ASSERT_NULL(node);
}

TEST(Ext4_Rmdir, RmdirNonEmptyFails,
     "rmdir() on a directory containing files returns non-zero") {
    WITH_EXT4(f);
    ASSERT_EQ(0, f.mkdir(f.root, "fulldir"));

    VfsNode* dir = f.find_root("fulldir");
    ASSERT_NOT_NULL(dir);
    ASSERT_EQ(0, f.create(dir, "content.txt"));
    Ext4Fixture::free_node(dir);

    ASSERT_NE(0, f.rmdir(f.root, "fulldir"));
}

TEST(Ext4_Rmdir, RmdirExistingImageDir,
     "rmdir() can remove an empty directory from the original image") {
    WITH_EXT4(f);
    // Create a fresh empty sub-dir to remove (don't remove subdir which has files).
    ASSERT_EQ(0, f.mkdir(f.root, "imgdir"));
    ASSERT_EQ(0, f.rmdir(f.root, "imgdir"));
    ASSERT_FALSE(f.list_contains(f.list_root(), "imgdir"));
}

TEST(Ext4_Rmdir, RmdirMissingReturnsError,
     "rmdir() on a non-existent directory returns non-zero") {
    WITH_EXT4(f);
    ASSERT_NE(0, f.rmdir(f.root, "ghost_dir"));
}

TEST(Ext4_Rmdir, RmdirNullParent,
     "rmdir() with null parent returns non-zero") {
    WITH_EXT4(f);
    ASSERT_NE(0, f.rmdir(nullptr, "subdir"));
}

TEST(Ext4_Rmdir, RmdirNullName,
     "rmdir() with null name returns non-zero") {
    WITH_EXT4(f);
    ASSERT_NE(0, f.rmdir(f.root, nullptr));
}

TEST(Ext4_Rmdir, MkdirRmdirCycleIsRepeatable,
     "mkdir → rmdir cycle can be repeated without corruption") {
    WITH_EXT4(f);
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(0, f.mkdir(f.root, "cycled"));
        ASSERT_EQ(0, f.rmdir(f.root, "cycled"));
    }
    ASSERT_FALSE(f.list_contains(f.list_root(), "cycled"));
}